/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_DB_H__
#define __HOBBES_DB_H__

#include <xpmem.h>

#include "hobbes_types.h"
#include "enclave.h"

/* set master db size to 64MB for now */
#define HDB_MASTER_DB_SIZE  (64 * 1024 * 1024) 
#define HDB_MASTER_DB_SEGID (1)



typedef void * hdb_db_t;

#define HDB_ENCLAVE      0
#define HDB_PROCESS      1
#define HDB_SEGMENT      2
#define HDB_ENCLAVE_HDR  3
#define HDB_NEXT_PROCESS 4





hdb_db_t hdb_create(u64 size);
hdb_db_t hdb_attach(void * db_addr);
void hdb_detach(hdb_db_t db);


static inline void * hdb_get_db_addr(hdb_db_t db) {
#ifdef USE_DATABASE_HANDLE
   return ((db_handle *)db)->db;
#else
    return db;
#endif
}




int hdb_insert_enclave(hdb_db_t       db, 
		       char         * name, 
		       int            mgmt_dev_id, 
		       enclave_type_t type, 
		       u64            parent);

int hdb_get_enclave_by_name(hdb_db_t                db, 
			    char                  * name, 
			    struct hobbes_enclave * enclave);

int hdb_delete_enclave(hdb_db_t db,
		       u64      enclave_id);

struct hobbes_enclave * hdb_get_enclave_list(hdb_db_t db, int * num_enclaves);
void hdb_free_enclave_list(struct hobbes_enclave * enclave_list);

#endif
