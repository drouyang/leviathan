/* Hobbes client program interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */



#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include <pet_log.h>

#include "hobbes.h"
#include "hobbes_util.h"
#include "hobbes_db.h"
#include "hobbes_enclave.h"
#include "hobbes_process.h"



hdb_db_t            hobbes_master_db = NULL;
static xemem_apid_t hobbes_db_apid;
static bool         hobbes_enabled   = false;


static void 
__attribute__((constructor))
hobbes_auto_init() {
    hobbes_id_t process_id = HOBBES_INVALID_ID;

    printf("Initializing Hobbes\n");

    /* Check is hobbes is available */
    if (!hobbes_is_available()) {
	ERROR("Hobbes is not available\n");
	return;
    }

    /* Check if we are a hobbes process */
    if (!getenv(HOBBES_ENV_PROCESS_ID)) {
	ERROR("This is not a Hobbes process\n");
	return;
    }

    process_id = hobbes_get_my_process_id();

    if (process_id == HOBBES_INVALID_ID) {
	ERROR("Invalid Hobbes Process ID\n");
	return;
    }

  
    /* Initialize hobbes */
    if (hobbes_client_init() == -1) {
	ERROR("Failed to initialize hobbes\n");
	return;
    }



    /* Hobbes is initialized, so we signal that we are now running */
    hobbes_set_process_state(process_id, PROCESS_RUNNING);
    
    /* Mark hobbes as enabled */
    hobbes_enabled = true;
}


static void 
__attribute__((destructor))
hobbes_auto_deinit()
{

    printf("Deinitializing Hobbes\n");

    if (!hobbes_enabled) {
	return;
    }

    hobbes_set_process_state(hobbes_get_my_process_id(), PROCESS_STOPPED);

    hobbes_client_deinit();

    hobbes_enabled = false;
}


int 
hobbes_client_init()
{
    void       * db_addr    = NULL;

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

    printf("Attaching to local database (db_addr=%p)\n", db_addr);

    hobbes_master_db  = hdb_attach(db_addr);

    if (hobbes_master_db == NULL) {
	ERROR("Error: Could not connect to database\n");
	return -1;
    }

    return 0;
}



int
hobbes_client_deinit()
{
    void * db_addr = NULL;

    db_addr = hdb_get_db_addr(hobbes_master_db);
    
    hdb_detach(hobbes_master_db);

    xemem_detach(db_addr);

    xemem_release(hobbes_db_apid);

    return 0;
}


bool 
hobbes_is_available( void )
{
    char * name = NULL;
    /* First check environment variables  (LWK init + Hobbes Apps)*/
    name = getenv(HOBBES_ENV_ENCLAVE_ID);

    if (name) {
	return true;
    }

    /* If not set, check for a hobbes device (Linux VM Init task) */

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
    char        * name = NULL;

    if (enclave_id == HOBBES_INVALID_ID) {
	return NULL;
    }

    name = hobbes_get_enclave_name(enclave_id);

    return name;
}

hobbes_id_t 
hobbes_get_my_process_id( void )
{
    hobbes_id_t  process_id = HOBBES_INVALID_ID;
    char       * id_str     = NULL;

    if (!hobbes_is_available()) {
	return HOBBES_INVALID_ID;
    }

    id_str = getenv(HOBBES_ENV_PROCESS_ID);

    if (id_str == NULL) {
	return HOBBES_INVALID_ID;
    }

    process_id = smart_atoi(HOBBES_INVALID_ID, id_str);

    return process_id;

}

char * 
hobbes_get_my_process_name( void )
{
    hobbes_id_t   process_id = hobbes_get_my_process_id();
    char        * name       = NULL;
    
    if (process_id == HOBBES_INVALID_ID) {
	return NULL;
    }

    name = hobbes_get_process_name(process_id);

    return name;
}
