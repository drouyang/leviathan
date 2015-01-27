/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_DB_H__
#define __HOBBES_DB_H__

#include <xpmem.h>

#include "hobbes_types.h"


/* set master db size to 64MB for now */
#define MASTER_DB_SIZE (64 * 1024 * 1024) 

typedef void * hdb_db_t;

#define HDB_ENCLAVE      0
#define HDB_PROCESS      1
#define HDB_SEGMENT      2
#define HDB_NEXT_ENCLAVE 3
#define HDB_NEXT_PROCESS 4


typedef enum {
    INVALID_ENCLAVE   = 0,
    MASTER_ENCLAVE    = 1,
    PISCES_ENCLAVE    = 2,
    PISCES_VM_ENCLAVE = 3,
    LINUX_VM_ENCLAVE  = 4
} enclave_type_t;

struct hdb_enclave {
    char name[32];

    u64 enclave_id;
    u64 parent_enclave_id;

    int mgmt_dev_id;
    
    enclave_type_t type;
};



hdb_db_t
hdb_attach(xpmem_segid_t segid, u64 size);

hdb_db_t
hdb_create(u64 size);


static inline void * hdb_get_db_addr(hdb_db_t db) {
#ifdef USE_DATABASE_HANDLE
   return ((db_handle *)db)->db;
#else
    return db;
#endif
}




int 
hdb_create_enclave(hdb_db_t db, char * name, int mgmt_dev_id, enclave_type_t type, u64 parent);





#endif
