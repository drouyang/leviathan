/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <xpmem.h>

#include "hobbes_db.h"
#include "hobbes_types.h"


#define PAGE_SIZE sysconf(_SC_PAGESIZE)

hdb_db_t 
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

hdb_db_t 
hdb_attach(void * db_addr) 
{ 
    hdb_db_t db = NULL;

    db = wg_attach_existing_local_database(db_addr);

    return db;
}    



static int
deserialize_enclave(hdb_db_t             db,
		    void               * enclave_rec,
		    struct hobbes_enclave * enclave)
{
    enclave->enclave_id  = wg_decode_int(db, wg_get_field(db, enclave_rec, 1));
    enclave->type        = wg_decode_int(db, wg_get_field(db, enclave_rec, 2));
    enclave->parent_id   = wg_decode_int(db, wg_get_field(db, enclave_rec, 3));
    enclave->mgmt_dev_id = wg_decode_int(db, wg_get_field(db, enclave_rec, 4));
    
    wg_decode_str_copy(db, wg_get_field(db, enclave_rec, 5), enclave->name, sizeof(enclave->name) - 1);

    return 0;
}



static struct hobbes_enclave * 
get_enclave_list(hdb_db_t   db,
		 int      * num_enclaves)
{
    struct hobbes_enclave * list    = NULL;

    void * db_rec  = NULL;
    void * hdr_rec = NULL;
    int    i       = 0;

    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE_HDR, NULL);    

    if (!hdr_rec) {
	printf("Error: malformed database. Missing enclave Header\n");
	return NULL;
    }

    *num_enclaves = wg_decode_int(db, wg_get_field(db, hdr_rec, 2));

    list = malloc(sizeof(struct hobbes_enclave) * (*num_enclaves));

    memset(list, 0, sizeof(struct hobbes_enclave) * (*num_enclaves));

    for (i = 0; i < *num_enclaves; i++) {
	db_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE, db_rec);
	
	if (!db_rec) {
	    printf("ERROR: Enclave Header state mismatch\n");
	    *num_enclaves = i;
	    break;
	}

	deserialize_enclave(db, db_rec, &(list[i]));
    }

    return list;
}


static int 
get_enclave_by_name(hdb_db_t             db, 
		    char               * name,
		    struct hobbes_enclave * enclave)
{
    void     * enclave_rec = NULL;
    wg_query * query       = NULL;
    int        ret         = 0;

    wg_query_arg arglist[2];    
 

    arglist[0].column = 0;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_ENCLAVE);    

    arglist[1].column = 5;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_str(db, name, NULL);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    enclave_rec = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    if (enclave_rec) {

	if (enclave) {
	    deserialize_enclave(db, enclave_rec, enclave);
	}

	return 1;	
    } 
	
    return 0;
}



static int 
insert_enclave(hdb_db_t         db,
	       char           * name, 
	       int              mgmt_dev_id, 
	       enclave_type_t   type, 
	       u64              parent)
{
    
    void * rec           = NULL;
    void * hdr_rec       = NULL;
    u64    enclave_id    = 0;
    u32    enclave_cnt   = 0;
    char   auto_name[32] = {[0 ... 31] = 0};

    
    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	printf("Error: malformed database. Missing enclave hEader\n");
	return -1;
    }
    
    /* Get Next Available enclave ID and enclave count */
    enclave_id  = wg_decode_int(db, wg_get_field(db, hdr_rec, 1));
    enclave_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, 2));
    
    if (name == NULL) {
	snprintf(auto_name, 31, "enclave-%lu", enclave_id);
	name = auto_name;
    }
    
    /* Insert enclave into the db */
    rec = wg_create_record(db, 6);
    wg_set_field(db, rec, 0, wg_encode_int(db, HDB_ENCLAVE));
    wg_set_field(db, rec, 1, wg_encode_int(db, enclave_id));
    wg_set_field(db, rec, 2, wg_encode_int(db, type));
    wg_set_field(db, rec, 3, wg_encode_int(db, parent));
    wg_set_field(db, rec, 4, wg_encode_int(db, mgmt_dev_id));
    wg_set_field(db, rec, 5, wg_encode_str(db, name, NULL));
    
    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, 1, wg_encode_int(db, enclave_id  + 1));
    wg_set_field(db, hdr_rec, 2, wg_encode_int(db, enclave_cnt + 1));

    return 0;
}




int 
hdb_get_enclave_by_id(u64                  enclave_id,
		      struct hobbes_enclave * endlave)
{


    return -1;
}


int 
hdb_get_enclave_by_name(hdb_db_t             db, 
			char               * name,
			struct hobbes_enclave * enclave)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	printf("Error: Could not lock database\n");
	return -1;
    }

    ret = get_enclave_by_name(db, name, enclave);

    if (!wg_end_read(db, lock_id)) {
	printf("Catastrophic database locking error\n");
	return -1;
    }

    return ret;
}


int
hdb_insert_enclave(hdb_db_t         db,
		   char           * name, 
		   int              mgmt_dev_id, 
		   enclave_type_t   type, 
		   u64              parent)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	printf("Error: Could not lock database\n");
	return -1;
    }

    ret = insert_enclave(db, name, mgmt_dev_id, type, parent);
    
    if (!wg_end_write(db, lock_id)) {
	printf("Error: Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}


int 
hdb_delete_enclave(u64 enclave_id)
{



    return -1;
}




struct hobbes_enclave * 
hdb_get_enclave_list(hdb_db_t   db,
		     int      * num_enclaves)
{
    struct hobbes_enclave * list = NULL;
    wg_int lock_id;

    if (!num_enclaves) {
	return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
	printf("Error: Could not lock database\n");
	return NULL;
    }

    list = get_enclave_list(db, num_enclaves);

    if (!wg_end_read(db, lock_id)) {
	printf("Catastrophic database locking error\n");
	return NULL;
    }

    return list;
}

void
hdb_free_enclave_list(struct hobbes_enclave * enclave_list)
{
    free(enclave_list);
}
