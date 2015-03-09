/* Hobbes Database interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <pet_log.h>
#include <stdint.h>

#include "hobbes_db.h"
#include "client.h"


#define PAGE_SIZE sysconf(_SC_PAGESIZE)

hdb_db_t 
hdb_create(uint64_t size) 
{
    void * db      = NULL;

    if (size % PAGE_SIZE) {
	ERROR("Database must be integral number of pages\n");
	return NULL;
    }

    db = wg_attach_local_database(size);

    if (db == NULL) {
	ERROR("Could not create database\n");
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

void 
hdb_detach(hdb_db_t db)
{

    wg_detach_database(db);
}



static void *
find_enclave_rec_by_id(hdb_db_t db, 
		       int      enclave_id) 
{
    void        * rec   = NULL;
    wg_query    * query = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = 0;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_ENCLAVE);    

    arglist[1].column = 1;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, enclave_id);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    rec = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return rec;
}


static int
deserialize_enclave(hdb_db_t                db,
		    void                  * enclave_rec,
		    struct hobbes_enclave * enclave)
{
    enclave->enclave_id  = wg_decode_int(db, wg_get_field(db, enclave_rec, 1));
    enclave->type        = wg_decode_int(db, wg_get_field(db, enclave_rec, 2));
    enclave->parent_id   = wg_decode_int(db, wg_get_field(db, enclave_rec, 3));
    enclave->mgmt_dev_id = wg_decode_int(db, wg_get_field(db, enclave_rec, 4));
    enclave->state       = wg_decode_int(db, wg_get_field(db, enclave_rec, 5));
    wg_decode_str_copy(db, wg_get_field(db, enclave_rec, 6), enclave->name, sizeof(enclave->name) - 1);



    return 0;
}


static int
update_enclave(hdb_db_t                db,
	       struct hobbes_enclave * enclave)
{
    void * enclave_rec = NULL;
    
    enclave_rec = find_enclave_rec_by_id(db, enclave->enclave_id);

    if (!enclave_rec) {
	printf("Could not find enclave to update\n");
	return -1;
    }

    wg_set_field(db, enclave_rec, 2, wg_encode_int(db, enclave->type));
    wg_set_field(db, enclave_rec, 3, wg_encode_int(db, enclave->parent_id));
    wg_set_field(db, enclave_rec, 4, wg_encode_int(db, enclave->mgmt_dev_id));
    wg_set_field(db, enclave_rec, 5, wg_encode_int(db, enclave->state));
    wg_set_field(db, enclave_rec, 6, wg_encode_str(db, enclave->name, NULL));

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
	ERROR("Malformed database. Missing enclave Header\n");
	return NULL;
    }

    *num_enclaves = wg_decode_int(db, wg_get_field(db, hdr_rec, 2));

    list = malloc(sizeof(struct hobbes_enclave) * (*num_enclaves));

    memset(list, 0, sizeof(struct hobbes_enclave) * (*num_enclaves));

    for (i = 0; i < *num_enclaves; i++) {
	db_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE, db_rec);
	
	if (!db_rec) {
	    ERROR("Enclave Header state mismatch\n");
	    *num_enclaves = i;
	    break;
	}

	deserialize_enclave(db, db_rec, &(list[i]));
    }

    return list;
}


static int 
get_enclave_by_name(hdb_db_t                db, 
		    char                  * name,
		    struct hobbes_enclave * enclave)
{
    void     * enclave_rec = NULL;
    wg_query * query       = NULL;
  
    wg_query_arg arglist[2];    
 
    arglist[0].column = 0;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_ENCLAVE);    

    arglist[1].column = 6;
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
get_enclave_by_id(hdb_db_t                db, 
		  int                     enclave_id,
		  struct hobbes_enclave * enclave)
{
    void     * enclave_rec = NULL;

    enclave_rec = find_enclave_rec_by_id(db, enclave_id);

    if (enclave_rec) {

	if (enclave) {
	    deserialize_enclave(db, enclave_rec, enclave);
	}

	return 1;	
    } 

    return 0;
}







static int 
create_enclave_record(hdb_db_t         db,
		      char           * name, 
		      int              mgmt_dev_id, 
		      enclave_type_t   type, 
		      int              parent)
{
    void    * rec           = NULL;
    void    * hdr_rec       = NULL;
    int       enclave_id    = 0;
    uint32_t  enclave_cnt   = 0;
    char      auto_name[32] = {[0 ... 31] = 0};

    
    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing enclave Header\n");
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
    rec = wg_create_record(db, 7);
    wg_set_field(db, rec, 0, wg_encode_int(db, HDB_ENCLAVE));
    wg_set_field(db, rec, 1, wg_encode_int(db, enclave_id));
    wg_set_field(db, rec, 2, wg_encode_int(db, type));
    wg_set_field(db, rec, 3, wg_encode_int(db, parent));
    wg_set_field(db, rec, 4, wg_encode_int(db, mgmt_dev_id));
    wg_set_field(db, rec, 5, wg_encode_int(db, ENCLAVE_INITTED));
    wg_set_field(db, rec, 6, wg_encode_str(db, name, NULL));
    
    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, 1, wg_encode_int(db, enclave_id  + 1));
    wg_set_field(db, hdr_rec, 2, wg_encode_int(db, enclave_cnt + 1));

    return enclave_id;
}

int
hdb_create_enclave(hdb_db_t         db,
		   char           * name, 
		   int              mgmt_dev_id, 
		   enclave_type_t   type, 
		   int              parent)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = create_enclave_record(db, name, mgmt_dev_id, type, parent);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}

static int
delete_enclave(hdb_db_t db,
	       int      enclave_id)
{
    void   * hdr_rec      = NULL;
    void   * rec          = NULL;
    uint32_t enclave_cnt  = 0;


    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("Malformed database. Missing enclave Header\n");
	return -1;
    }
    
    rec = find_enclave_rec_by_id(db, enclave_id);
  
    if (!rec) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    if (wg_delete_record(db, rec) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    enclave_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, 2));
    wg_set_field(db, hdr_rec, 2, wg_encode_int(db, enclave_cnt - 1));

    return 0;
}


int 
hdb_get_enclave_by_name(hdb_db_t                db, 
			char                  * name,
			struct hobbes_enclave * enclave)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = get_enclave_by_name(db, name, enclave);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return ret;
}


int 
hdb_get_enclave_by_id(hdb_db_t                db, 
		      int                     enclave_id,
		      struct hobbes_enclave * enclave)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = get_enclave_by_id(db, enclave_id, enclave);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return ret;
}






int
hdb_update_enclave(hdb_db_t                db,
		   struct hobbes_enclave * enclave)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = update_enclave(db, enclave);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}


int 
hdb_delete_enclave(hdb_db_t db,
		   int      enclave_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = delete_enclave(db, enclave_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
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
	ERROR("Could not lock database\n");
	return NULL;
    }

    list = get_enclave_list(db, num_enclaves);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return list;
}


void
hdb_free_enclave_list(struct hobbes_enclave * enclave_list)
{
    free(enclave_list);
}


static void *
find_xpmem_rec_by_segid(hdb_db_t      db, 
                        xpmem_segid_t segid) 
{
    void        * rec   = NULL;
    wg_query    * query = NULL;
    wg_query_arg  arglist[2];

    /* Convert segid to string (TODO: can the db encode 64 bit values automatically?) */
    arglist[0].column = 0;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_XPMEM_SEGMENT);    

    arglist[1].column = 1;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, segid);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    rec = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return rec;
}

static void *
find_xpmem_rec_by_name(hdb_db_t db,
                       char   * name)
{
    void        * rec   = NULL;
    wg_query    * query = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = 0;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_XPMEM_SEGMENT);

    arglist[1].column = 2;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_str(db, name, NULL);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    rec = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return rec;

}

static int 
create_xpmem_record(hdb_db_t      db,
                    xpmem_segid_t segid,
                    char        * name)
{
    void * rec           = NULL;
    void * hdr_rec       = NULL;
    int    segment_cnt   = 0;

    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_XPMEM_HDR, NULL);
    
    if (!hdr_rec) {
        ERROR("malformed database. Missing xpmem Header\n");
        return -1;
    }

    /* Ensure segid and name do not exist */
    rec = find_xpmem_rec_by_segid(db, segid);
    if (rec) {
        ERROR("xpmem segment with segid %lli already present\n", segid);
        return -1;
    }

    rec = find_xpmem_rec_by_name(db, name);
    if (rec) {
        ERROR("xpmem segment with name %s already present\n", name);
        return -1;
    }

    /* Insert segment into the db */
    rec = wg_create_record(db, 3);
    wg_set_field(db, rec, 0, wg_encode_int(db, HDB_XPMEM_SEGMENT));
    wg_set_field(db, rec, 1, wg_encode_int(db, segid));
    wg_set_field(db, rec, 2, wg_encode_str(db, name, NULL));

    /* Update the xpmem Header information */
    segment_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, 1));
    wg_set_field(db, hdr_rec, 1, wg_encode_int(db, segment_cnt + 1));

    return 0;
}

static int
delete_xpmem_record(hdb_db_t      db,
                    xpmem_segid_t segid,
                    char        * name)
{
    void * rec           = NULL;
    void * hdr_rec       = NULL;
    int    segment_cnt   = 0;
    int    ret           = 0;

    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_XPMEM_HDR, NULL);
    
    if (!hdr_rec) {
        ERROR("malformed database. Missing xpmem Header\n");
        return -1;
    }

    /* Find record */
    if (segid > 0) {
        rec = find_xpmem_rec_by_segid(db, segid);
        if (!rec) {
            ERROR("Could not find xpmem segment (segid: %lli)\n", segid);
            return -1;
        }
    } else if (name) {
        rec = find_xpmem_rec_by_name(db, name);
        if (!rec) {
            ERROR("Could not find xpmem segment (name: %s)\n", name);
            return -1;
        }
    } else {
        ERROR("Supply valid segid or name\n");
        return -1;
    }

    ret = wg_delete_record(db, rec);
    if (ret != 0) {
        ERROR("Could not delete xpmem segment from database\n");
        return ret;
    }

    /* Update the xpmem Header information */
    segment_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, 1));
    wg_set_field(db, hdr_rec, 1, wg_encode_int(db, segment_cnt - 1));

    return 0;
}

static int
deserialize_segment(hdb_db_t                db,
                    void                  * segment_rec,
                    struct hobbes_segment * segment)
{
    segment->segid = wg_decode_int(db, wg_get_field(db, segment_rec, 1));
    wg_decode_str_copy(db, wg_get_field(db, segment_rec, 2), segment->name, sizeof(segment->name) - 1);

    return 0;
}

static struct hobbes_segment * 
get_segment_list(hdb_db_t db,
                 int    * num_segments)
{
    struct hobbes_segment * list = NULL;

    void * db_rec  = NULL;
    void * hdr_rec = NULL;
    int    i       = 0;

    hdr_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_XPMEM_HDR, NULL);    

    if (!hdr_rec) {
        ERROR("Malformed database. Missing xpmem Header\n");
        return NULL;
    }

    *num_segments = wg_decode_int(db, wg_get_field(db, hdr_rec, 1));

    list = malloc(sizeof(struct hobbes_segment) * (*num_segments));
    if (!list)
        return NULL;

    memset(list, 0, sizeof(struct hobbes_segment) * (*num_segments));

    for (i = 0; i < *num_segments; i++) {
        db_rec = wg_find_record_int(db, 0, WG_COND_EQUAL, HDB_XPMEM_SEGMENT, db_rec);
        
        if (!db_rec) {
            ERROR("xpmem Header state mismatch\n");
            *num_segments = i;
            break;
        }

        deserialize_segment(db, db_rec, &(list[i]));
    }

    return list;
}

int
hdb_export_segment(hdb_db_t      db,
                   xpmem_segid_t segid,
                   char        * name)
{
    wg_int lock_id;
    int    ret;

    lock_id = wg_start_write(db);
    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    ret = create_xpmem_record(db, segid, name);
    if (ret != 0) 
        ERROR("Could not create xpmem database record\n");

out:
    if (wg_end_write(db, lock_id) == 0)
        ERROR("Apparently this is catastrophic...\n");

    return ret;
}

int
hdb_remove_segment(hdb_db_t      db,
                   xpmem_segid_t segid,
                   char        * name)
{
    wg_int lock_id;
    int    ret;
    
    lock_id = wg_start_write(db);
    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    ret = delete_xpmem_record(db, segid, name);
    if (ret != 0)
        ERROR("Could not delete xpmem database record\n");

out:
    if (wg_end_write(db, lock_id) == 0)
        ERROR("Apparently this is catastrophic...\n");

    return ret;

}

int
hdb_get_segment_by_name(hdb_db_t                db,
                        char                  * name,
                        struct hobbes_segment * segment)
{
    wg_int lock_id;
    void * rec;
    int ret = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    rec = find_xpmem_rec_by_name(db, name);

    if (rec)
        deserialize_segment(db, rec, segment);
    else 
        ret = -1;

    if (!wg_end_read(db, lock_id))
        ERROR("Catastrophic database locking error\n");

    return ret;
}

struct hobbes_segment *
hdb_get_segment_list(hdb_db_t db,
                     int    * num_segments)
{
    struct hobbes_segment * list = NULL;
    wg_int lock_id;

    if (!num_segments) {
        return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return NULL;
    }

    list = get_segment_list(db, num_segments);

    if (!wg_end_read(db, lock_id))
        ERROR("Catastrophic database locking error\n");

    return list;
}
void
hdb_free_segment_list(struct hobbes_segment* segment_list)
{
    free(segment_list);
}
