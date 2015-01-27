/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>

#include "hobbes_db.h"
#include "hobbes_types.h"


#define PAGE_SIZE sysconf(_SC_PAGESIZE)

void * 
hdb_create(u64 size) 
{
    void * db      = NULL;

    if (size % PAGE_SIZE) {
	printf("Error: Database must be integral number of pages\n");
	return NULL;
    }

    db = wg_attach_local_database(size);

    if (db == NULL) {
	printf("Could not create database\n");
	return NULL;
    }

    return db;
}



int
hdb_create_enclave(hdb_db_t         db,
		   char           * name, 
		   int              mgmt_dev_id, 
		   enclave_type_t   type, 
		   u64              parent)
{
    wg_int lock_id;
    int ret = -1;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	printf("Error: Could not lock database\n");
	return -1;
    }


    {
	void * rec    = NULL;
	void * id_rec = NULL;
	u64 enclave_id = 0;

	id_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_NEXT_ENCLAVE, NULL);

	if (!id_rec) {
	    printf("Error: malformed database. Missing enclave ID counter\n");
	    goto unlock;
	}

	/* Get Next Available enclave ID and increment counter */
	enclave_id = wg_decode_int(db, wg_get_field(db, id_rec, 1));
	wg_set_field(db, id_rec, 1, wg_encode_int(db, enclave_id + 1));

	/* Insert enclave into the db */
	rec = wg_create_record(db, 6);
	wg_set_field(db, rec, 0, wg_encode_int(db, HDB_ENCLAVE));
	wg_set_field(db, rec, 1, wg_encode_int(db, enclave_id));
	wg_set_field(db, rec, 2, wg_encode_int(db, type));
	wg_set_field(db, rec, 3, wg_encode_int(db, parent));
	wg_set_field(db, rec, 4, wg_encode_int(db, mgmt_dev_id));
	wg_set_field(db, rec, 5, wg_encode_str(db, name, NULL));

    }
    
    unlock: 
    if (!wg_end_write(db, lock_id)) {
	printf("Error: Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}
