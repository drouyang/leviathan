/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */




#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include <pet_log.h>

#include "hobbes.h"
#include "hobbes_util.h"
#include "hobbes_db.h"
#include "hobbes_enclave.h"
#include "hobbes_app.h"

#define HOBBES_CPUID_LEAF (0x41000000)
#define HOBBES_MAGIC      (0x40bbe5)
#define HOBBES_VERSION    (1)


hdb_db_t            hobbes_master_db = NULL;
static xemem_apid_t hobbes_db_apid;
static bool         hobbes_enabled   = false;
static pid_t        hobbes_pid       = 0;


static int
cpuid(uint32_t   op,
      uint32_t * eax, 
      uint32_t * ebx, 
      uint32_t * ecx, 
      uint32_t * edx)
{
  
    __asm__("cpuid\r\n"
	    : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
	    : "0" (op)
	    :);
    
    return 0;
}


static void 
__attribute__((constructor))
hobbes_app_auto_init() {
    hobbes_id_t app_id = HOBBES_INVALID_ID;

    app_id = hobbes_get_my_app_id();

    if ((app_id == HOBBES_INVALID_ID) ||
	(app_id == HOBBES_INIT_APP_ID)) {
	return;
    }

    DEBUG("Hobbes: Initializing\n");
  
    /* Initialize hobbes */
    if (hobbes_client_init() == -1) {
	DEBUG("Failed to initialize hobbes application\n");
	return;
    }

    /* Hobbes is initialized, so we signal that we are now running */
    hobbes_set_app_state(app_id, APP_RUNNING);
    
    /* Mark hobbes as enabled */
    hobbes_enabled = true;
}


static void 
__attribute__((destructor))
hobbes_app_auto_deinit()
{

    //    DEBUG("Hobbes: Deinitializing\n");

    if (!hobbes_enabled) {
	return;
    }

    if (getpid() != hobbes_pid) {
	ERROR("App %d forked a child that did not call hobbes_client_init()!\n", hobbes_get_my_app_id());
	return;
    }

    hobbes_set_app_state(hobbes_get_my_app_id(), APP_STOPPED);

    hobbes_client_deinit();

    hobbes_enabled = false;
}


int 
hobbes_client_init()
{
    void       * db_addr    = NULL;

    assert(!hobbes_is_master_inittask());

    hobbes_db_apid = xemem_get(HDB_MASTER_DB_SEGID, XEMEM_RDWR);

    if (hobbes_db_apid <= 0) {
        ERROR("xpmem get failed\n");
	return -1;
    }


    {
	struct xemem_addr addr;

        addr.apid   = hobbes_db_apid;
        addr.offset = 0;

        db_addr = xemem_attach(addr, HDB_MASTER_DB_SIZE, NULL);
        if (db_addr == MAP_FAILED) {
            ERROR("xpmem attach failed\n");
            xemem_release(hobbes_db_apid);
            return -1;
        }
    }

    //    printf("Attaching to local database (db_addr=%p)\n", db_addr);

    hobbes_master_db  = hdb_attach(db_addr);

    if (hobbes_master_db == NULL) {
	ERROR("Error: Could not connect to database\n");
	return -1;
    }

    /* Save process pid to prevent fork'ed processes from touching the DB on teardown */
    hobbes_pid = getpid();

    return 0;
}



int
hobbes_client_deinit()
{
    void * db_addr = NULL;

    assert(!hobbes_is_master_inittask());

    db_addr = hdb_get_db_addr(hobbes_master_db);
    
    hdb_detach(hobbes_master_db);

    xemem_detach(db_addr);

    xemem_release(hobbes_db_apid);

    return 0;
}

bool 
hobbes_is_app( void )
{
    hobbes_id_t app_id = hobbes_get_my_app_id();

    return (app_id != HOBBES_INVALID_ID);
}


bool
hobbes_is_client_inittask( void )
{

    hobbes_id_t enclave_id = hobbes_get_my_enclave_id();
    hobbes_id_t app_id     = hobbes_get_my_app_id();

    if (enclave_id == HOBBES_INVALID_ID) {
	return false;
    }

    if (enclave_id == HOBBES_MASTER_ENCLAVE_ID) {
	return false;
    }

    if (app_id == HOBBES_INIT_APP_ID) {
	return true;
    }

    return false;
}

bool 
hobbes_is_master_inittask( void )
{
    hobbes_id_t enclave_id = hobbes_get_my_enclave_id();
    hobbes_id_t app_id     = hobbes_get_my_app_id();

    if (enclave_id != HOBBES_MASTER_ENCLAVE_ID) {
	return false;
    }

    if (app_id == HOBBES_INIT_APP_ID) {
	return true;
    }

    return false;
}


bool 
hobbes_is_available( void )
{
    char * id_str = NULL;

    /* First check environment variables  (LWK init + Hobbes Apps)*/
    id_str = getenv(HOBBES_ENV_ENCLAVE_ID);
    
    if (id_str) {
	return true;
    }

    /* If not set, check for a Hobbes CPUID (VM environment) */
    {
	uint32_t eax = 0;
	uint32_t ebx = 0;
	uint32_t ecx = 0;
	uint32_t edx = 0;
	
	cpuid(HOBBES_CPUID_LEAF, &eax, &ebx, &ecx, &edx);
	
	if (ebx == HOBBES_MAGIC) {
	    char * tmp_str = NULL;
	    
	    asprintf(&tmp_str, "%u", edx);	    
	    setenv(HOBBES_ENV_ENCLAVE_ID, tmp_str, 0);
	    free(tmp_str);

	    return true;
	}
    }

    return false;
}

bool
hobbes_is_enabled( void )
{
    return hobbes_enabled;
}


hobbes_id_t
hobbes_get_my_enclave_id( void )
{
    hobbes_id_t  enclave_id = HOBBES_INVALID_ID;
    char       * id_str     = NULL;

    if (!hobbes_is_available()) {
	return HOBBES_INVALID_ID;
    }
    
    id_str = getenv(HOBBES_ENV_ENCLAVE_ID);

    if (id_str == NULL) {
	ERROR("Hobbes: Missing enclave ID environment variable\n");
	return HOBBES_INVALID_ID;
    }

    enclave_id = smart_atoi(HOBBES_INVALID_ID, id_str);

    return enclave_id;
}

char *
hobbes_get_my_enclave_name( void )
{
    hobbes_id_t   enclave_id = hobbes_get_my_enclave_id();
    char        * name       = NULL;

    if (enclave_id == HOBBES_INVALID_ID) {
	return NULL;
    }

    name = hobbes_get_enclave_name(enclave_id);

    return name;
}

hobbes_id_t 
hobbes_get_my_app_id( void )
{
    hobbes_id_t  app_id = HOBBES_INVALID_ID;
    char       * id_str = NULL;

    if (!hobbes_is_available()) {
	return HOBBES_INVALID_ID;
    }

    id_str = getenv(HOBBES_ENV_APP_ID);

    if (id_str == NULL) {
	return HOBBES_INVALID_ID;
    }

    app_id = smart_atoi(HOBBES_INVALID_ID, id_str);

    return app_id;

}

char * 
hobbes_get_my_app_name( void )
{
    hobbes_id_t   app_id = hobbes_get_my_app_id();
    char        * name   = NULL;
    
    if (app_id == HOBBES_INVALID_ID) {
	return NULL;
    }

    name = hobbes_get_app_name(app_id);

    return name;
}
