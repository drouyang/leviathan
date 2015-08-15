#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "pmi.h"
#include "hobbes_db.h"

#define PMI_MAX_STRING_LEN 128

extern hdb_db_t hobbes_master_db;

/* Set PMI_initialized to 1 for singleton init but no process manager
 * to help.  Initialized to 2 for normal initialization.  Initialized
 * to values higher than 2 when singleton_init by a process manager.
 * All values higher than 1 invlove a PM in some way. */
typedef enum {
    PMI_UNINITIALIZED   = 0,
    NORMAL_INIT_WITH_PM = 2,
} PMIState;

/* PMI client state */
static PMIState PMI_initialized = PMI_UNINITIALIZED;
static int      PMI_size = -1;
static int      PMI_rank = -1;
static int      PMI_kvsname_max = 0;
static int      PMI_keylen_max = 0;
static int      PMI_vallen_max = 0;
//static int      PMI_debug = 0;
//static int      PMI_spawned = 0;


int
PMI_Init(int *spawned)
{
    char *p;

    PMI_initialized = PMI_UNINITIALIZED;

    if ((p = getenv("PMI_SIZE")) != NULL) {
        PMI_size = atoi(p);
    } else {
        printf("Failed to get PMI_SIZE environment variable.\n");
        return PMI_FAIL;
    }

    if ((p = getenv("PMI_RANK")) != NULL) {
        PMI_rank = atoi(p);
    } else {
        printf("Failed to get PMI_RANK environment variable.\n");
        return PMI_FAIL;
    }

    if (PMI_KVS_Get_name_length_max(&PMI_kvsname_max) != PMI_SUCCESS)
        return PMI_FAIL;

    if (PMI_KVS_Get_key_length_max(&PMI_keylen_max) != PMI_SUCCESS)
        return PMI_FAIL;

    if (PMI_KVS_Get_value_length_max(&PMI_vallen_max) != PMI_SUCCESS)
        return PMI_FAIL;

    if (spawned)
        *spawned = PMI_FALSE;

    PMI_initialized = NORMAL_INIT_WITH_PM;

    return PMI_SUCCESS;
}


int
PMI_Initialized(int *initialized)
{
    *initialized = (PMI_initialized != 0);
    return PMI_SUCCESS;
}


int
PMI_Finalize(void)
{
    // TODO: detach from whitedb	
    return PMI_SUCCESS;
}


int
PMI_Get_size(int *size)
{
    *size = PMI_size;
    return PMI_SUCCESS;
}


int
PMI_Get_rank(int *rank)
{
    *rank = PMI_rank;
    return PMI_SUCCESS;
}


int
PMI_Get_universe_size(int *size)
{
    *size = PMI_size;	
    return PMI_SUCCESS;
}


//TODO: Make this real
int
PMI_Get_appnum(int *appnum)
{
    if (appnum)
        *appnum = 0;

    return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Publish_name(const char service_name[], const char port[])
{
    return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Unpublish_name(const char service_name[])
{
    return PMI_SUCCESS;
}


//TODO: Ignore for now
int
PMI_Lookup_name(const char service_name[], char port[])
{
    return PMI_SUCCESS;
}


//TODO: Make this real
int
PMI_Barrier(void)
{
    printf("Made it into barrier\n");
    return PMI_SUCCESS;
}


int
PMI_Abort(int exit_code, const char error_msg[])
{
    printf("aborting job:\n%s\n", error_msg);
    exit(exit_code);
    return -1;
}


//The KVS space name may not be actively used (we may not need more than one)
int
PMI_KVS_Get_my_name(char kvsname[], int length)
{
    strcpy(kvsname, "place_holder_kvsname");
    return PMI_SUCCESS;
}


int
PMI_KVS_Get_name_length_max(int *length)
{
    if (length)
        *length = PMI_MAX_STRING_LEN;

    return PMI_SUCCESS;
}


int
PMI_KVS_Get_key_length_max(int *length)
{
    if (length)
        *length = PMI_MAX_STRING_LEN;

    return PMI_SUCCESS;
}


int
PMI_KVS_Get_value_length_max(int *length)
{
    if (length)
        *length = PMI_MAX_STRING_LEN;

    return PMI_SUCCESS;
}


int
PMI_KVS_Put(
    const char kvsname[],
    const char key[],
    const char value[]
)
{
    /* TODO: Use the real Hobbes AppID rather than '0' (second arg) */
    if (hdb_put_pmi_keyval(hobbes_master_db, 0, kvsname, key, value) != 0)
	return PMI_FAIL;

    return PMI_SUCCESS;
}


int
PMI_KVS_Commit(const char kvsname[])
{
    return PMI_SUCCESS;
}


int
PMI_KVS_Get(
    const char kvsname[],
    const char key[],
    char       value[],
    int        length
)
{
    const char * db_val;

    if (hdb_get_pmi_keyval(hobbes_master_db, 0, kvsname, key, &db_val) != 0)
        return PMI_FAIL;

    strncpy(value, db_val, length);
    if (length > 0)
	value[length - 1] = '\0';

    return PMI_SUCCESS;
}


int
PMI_Spawn_multiple(int count,
                   const char * cmds[],
                   const char ** argvs[],
                   const int maxprocs[],
                   const int info_keyval_sizesp[],
                   const PMI_keyval_t * info_keyval_vectors[],
                   int preput_keyval_size,
                   const PMI_keyval_t preput_keyval_vector[],
                   int errors[])
{
    return PMI_FAIL;
}
