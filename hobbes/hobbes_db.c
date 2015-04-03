/* Hobbes Database interface for whitedb
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dbapi.h>
#include <dballoc.h>

#include <pet_log.h>
#include <stdint.h>

#include "hobbes_db.h"
#include "client.h"


/* 
 * Database Record Type
 *    Each row in the database has an associated type from the list below
 */
#define HDB_REC_ENCLAVE          0
#define HDB_REC_PROCESS          1
#define HDB_REC_SEGMENT          2
#define HDB_REC_ENCLAVE_HDR      3
#define HDB_REC_NEXT_PROCESS     4
#define HDB_REC_XEMEM_HDR        5
#define HDB_REC_XEMEM_SEGMENT    6
#define HDB_REC_XEMEM_ATTACHMENT 7




/*
 * Database Field definitions
 *     The column types for each row are specified below
 */

/* The first column of every row specifies the row type */
#define HDB_TYPE_FIELD       0 

/* Columns for enclave header */
#define HDB_ENCLAVE_HDR_NEXT 1
#define HDB_ENCLAVE_HDR_CNT  2

/* Columns for enclave records */
#define HDB_ENCLAVE_ID       1
#define HDB_ENCLAVE_TYPE     2
#define HDB_ENCLAVE_DEV_ID   3
#define HDB_ENCLAVE_STATE    4
#define HDB_ENCLAVE_NAME     5
#define HDB_ENCLAVE_CMDQ_ID  6
#define HDB_ENCLAVE_PARENT   7

/* Columns for XEMEM segment header */
#define HDB_SEGMENT_HDR_CNT  1

/* Columns for XEMEM segment records */
#define HDB_SEGMENT_SEGID    1
#define HDB_SEGMENT_NAME     2


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

    wg_detach_local_database(db);
}


int
hdb_init_master_db(hdb_db_t db)
{
    void * rec = NULL;
    
    rec = wg_create_record(db, 3);
    wg_set_field(db, rec, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_ENCLAVE_HDR));
    wg_set_field(db, rec, HDB_ENCLAVE_HDR_NEXT, wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_ENCLAVE_HDR_CNT , wg_encode_int(db, 0));
    
    rec = wg_create_record(db, 2);
    wg_set_field(db, rec, HDB_TYPE_FIELD, wg_encode_int(db, HDB_REC_NEXT_PROCESS));
    wg_set_field(db, rec, 1, wg_encode_int(db, 0));
    
    /* Create XEMEM header */
    rec = wg_create_record(db, 2);
    wg_set_field(db, rec, HDB_TYPE_FIELD,      wg_encode_int(db, HDB_REC_XEMEM_HDR));
    wg_set_field(db, rec, HDB_SEGMENT_HDR_CNT, wg_encode_int(db, 0));

    return 0;
}


/* 
 * Enclave Accessors 
 */


/**
 * Get an enclave handle from an enclave id
 *  - Returns NULL if no enclave is found 
 **/
static hdb_enclave_t
__get_enclave_by_id(hdb_db_t db, 
		    hdb_id_t enclave_id) 
{
    hdb_enclave_t enclave  = NULL;
    wg_query    * query    = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_ENCLAVE);    

    arglist[1].column = HDB_ENCLAVE_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, enclave_id);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    enclave = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return enclave;
}





/**
 * Get an enclave handle from an enclave name
 *  - Returns NULL if no enclave is found 
 **/
static hdb_enclave_t
__get_enclave_by_name(hdb_db_t   db, 
		      char     * name)
{
    hdb_enclave_t   enclave  = NULL;
    wg_query      * query    = NULL;
    wg_query_arg    arglist[2];    
 
    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_ENCLAVE);    

    arglist[1].column = HDB_ENCLAVE_NAME;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_str(db, name, NULL);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    enclave = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return enclave;
}





static hdb_id_t
__create_enclave_record(hdb_db_t         db,
			char           * name, 
			int              mgmt_dev_id, 
			enclave_type_t   type, 
			hdb_id_t         parent)
{
    void    * hdr_rec       = NULL;
    hdb_id_t  enclave_id    = 0;
    uint32_t  enclave_cnt   = 0;
    char      auto_name[32] = {[0 ... 31] = 0};

    hdb_enclave_t enclave   = NULL;

    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing enclave Header\n");
	return -1;
    }
    
    /* Get Next Available enclave ID and enclave count */
    enclave_id  = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_NEXT));
    enclave_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT));
    
    if (name == NULL) {
	snprintf(auto_name, 31, "enclave-%lu", enclave_id);
	name = auto_name;
    }
    
    /* Insert enclave into the db */
    enclave = wg_create_record(db, 8);
    wg_set_field(db, enclave, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_ENCLAVE));
    wg_set_field(db, enclave, HDB_ENCLAVE_ID,       wg_encode_int(db, enclave_id));
    wg_set_field(db, enclave, HDB_ENCLAVE_TYPE,     wg_encode_int(db, type));
    wg_set_field(db, enclave, HDB_ENCLAVE_DEV_ID,   wg_encode_int(db, mgmt_dev_id));
    wg_set_field(db, enclave, HDB_ENCLAVE_STATE,    wg_encode_int(db, ENCLAVE_INITTED));
    wg_set_field(db, enclave, HDB_ENCLAVE_NAME,     wg_encode_str(db, name, NULL));
    wg_set_field(db, enclave, HDB_ENCLAVE_CMDQ_ID,  wg_encode_int(db, 0));
    wg_set_field(db, enclave, HDB_ENCLAVE_PARENT,   wg_encode_int(db, parent));

    
    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, HDB_ENCLAVE_HDR_NEXT, wg_encode_int(db, enclave_id  + 1));
    wg_set_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT,  wg_encode_int(db, enclave_cnt + 1));

    return enclave_id;
}

hdb_id_t
hdb_create_enclave(hdb_db_t         db,
		   char           * name, 
		   int              mgmt_dev_id, 
		   enclave_type_t   type, 
		   hdb_id_t         parent)
{
    wg_int   lock_id;
    hdb_id_t enclave_id = -1;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    enclave_id = __create_enclave_record(db, name, mgmt_dev_id, type, parent);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return enclave_id;
}

static int
__delete_enclave(hdb_db_t db,
		 hdb_id_t enclave_id)
{
    uint32_t      enclave_cnt = 0;
    void        * hdr_rec     = NULL;
    hdb_enclave_t enclave     = NULL;


    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("Malformed database. Missing enclave Header\n");
	return -1;
    }
    
    enclave = __get_enclave_by_id(db, enclave_id);
  
    if (!enclave) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    if (wg_delete_record(db, enclave) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    enclave_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT));
    wg_set_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT, wg_encode_int(db, enclave_cnt - 1));

    return 0;
}



int 
hdb_delete_enclave(hdb_db_t db,
		   hdb_id_t enclave_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __delete_enclave(db, enclave_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
 }



static int 
__get_enclave_dev_id(hdb_db_t db, 
		     hdb_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    int dev_id = 0;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    dev_id = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_DEV_ID));

    return dev_id;
}

int 
hdb_get_enclave_dev_id(hdb_db_t db, 
		       hdb_id_t enclave_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __get_enclave_dev_id(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}


static int 
__set_enclave_dev_id(hdb_db_t db, 
		     hdb_id_t enclave_id, 
		     int      dev_id)
{
    hdb_enclave_t enclave = NULL;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    wg_set_field(db, enclave, HDB_ENCLAVE_DEV_ID, wg_encode_int(db, dev_id));

    return 0;
}

int 
hdb_set_enclave_dev_id(hdb_db_t db, 
		       hdb_id_t enclave_id, 
		       int      dev_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __set_enclave_dev_id(db, enclave_id, dev_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}


static enclave_type_t
__get_enclave_type(hdb_db_t db, 
		   hdb_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    enclave_type_t type = INVALID_ENCLAVE;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    type = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_TYPE));

    return type;
}

enclave_type_t
hdb_get_enclave_type(hdb_db_t db, 
		     hdb_id_t enclave_id)
{
    wg_int lock_id;
    enclave_type_t type = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    type = __get_enclave_type(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return type;
}




static enclave_state_t
__get_enclave_state(hdb_db_t db, 
		    hdb_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    enclave_state_t state = ENCLAVE_ERROR;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    state = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_STATE));

    return state;
}

enclave_state_t
hdb_get_enclave_state(hdb_db_t db, 
		      hdb_id_t enclave_id)
{
    wg_int lock_id;
    enclave_state_t state = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    state = __get_enclave_state(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return state;
}


static int
__set_enclave_state(hdb_db_t        db, 
		    hdb_id_t        enclave_id, 
		    enclave_state_t state)
{
    hdb_enclave_t enclave = NULL;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    wg_set_field(db, enclave, HDB_ENCLAVE_STATE, wg_encode_int(db, state));

    return 0;
}

int
hdb_set_enclave_state(hdb_db_t        db, 
		      hdb_id_t        enclave_id, 
		      enclave_state_t state)
{
    wg_int lock_id;
    int ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __set_enclave_state(db, enclave_id, state);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}



static char *
__get_enclave_name(hdb_db_t db, 
		   hdb_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    char * name = NULL;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return NULL;
    }

    name = wg_decode_str(db, wg_get_field(db, enclave, HDB_ENCLAVE_NAME));

    return name;
}

char * 
hdb_get_enclave_name(hdb_db_t db, 
		     hdb_id_t enclave_id)
{
    wg_int lock_id;
    char * name = NULL;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

   name = __get_enclave_name(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return NULL;
    }
    
    return name;
}


static xemem_segid_t
__get_enclave_cmdq(hdb_db_t db, 
		   hdb_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;
    xemem_segid_t segid   = 0;


    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    segid = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_CMDQ_ID));

    return segid;
}

xemem_segid_t 
hdb_get_enclave_cmdq(hdb_db_t db,
		     hdb_id_t enclave_id)
{
    wg_int lock_id;
    xemem_segid_t segid = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    segid = __get_enclave_cmdq(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return segid;
}

static int
__set_enclave_cmdq(hdb_db_t      db,
		   hdb_id_t      enclave_id, 
		   xemem_segid_t segid)
{
    hdb_enclave_t enclave = NULL;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return -1;
    }

    wg_set_field(db, enclave, HDB_ENCLAVE_CMDQ_ID, wg_encode_int(db, segid));

    return 0;
}

int
hdb_set_enclave_cmdq(hdb_db_t      db,
		     hdb_id_t      enclave_id, 
		     xemem_segid_t segid)
{
    wg_int lock_id;
    int ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __set_enclave_cmdq(db, enclave_id, segid);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}



static hdb_id_t
__get_enclave_id(hdb_db_t   db, 
		 char     * enclave_name)
{
    hdb_enclave_t enclave    = NULL;
    hdb_id_t      enclave_id = -1;

    enclave = __get_enclave_by_name(db, enclave_name);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (name: %s)\n", enclave_name);
	return -1;
    }

    enclave_id = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_ID));

    return enclave_id;
}

hdb_id_t
hdb_get_enclave_id(hdb_db_t   db, 
		   char     * enclave_name)
{
    wg_int   lock_id;
    hdb_id_t enclave_id = -1;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

   enclave_id = __get_enclave_id(db, enclave_name);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return enclave_id;
}

static hdb_id_t *
__get_enclaves(hdb_db_t   db,
	      int       * num_enclaves)
{
    hdb_id_t * id_arr  = NULL;
    void     * db_rec  = NULL;
    void     * hdr_rec = NULL;
    int        cnt     = 0;
    int        i       = 0;
    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE_HDR, NULL);    

    if (!hdr_rec) {
	ERROR("Malformed database. Missing enclave Header\n");
	return NULL;
    }

    cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT));

    id_arr = calloc(sizeof(hdb_id_t), cnt);

    for (i = 0; i < cnt; i++) {
	db_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE, db_rec);
	

	if (!db_rec) {
	    ERROR("Enclave Header state mismatch\n");
	    cnt = i;
	    break;
	}

	id_arr[i] = wg_decode_int(db, wg_get_field(db, db_rec, HDB_ENCLAVE_ID));

    }

    *num_enclaves = cnt;
    return id_arr;
}


hdb_id_t * 
hdb_get_enclaves(hdb_db_t   db,
		 int      * num_enclaves)
{
    hdb_id_t * id_arr = NULL;
    wg_int     lock_id;

    if (!num_enclaves) {
	return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    id_arr = __get_enclaves(db, num_enclaves);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return id_arr;
}
















static hdb_segment_t
__get_segment_by_segid(hdb_db_t      db, 
		       xemem_segid_t segid) 
{
    hdb_segment_t segment = NULL;
    wg_query    * query   = NULL;
    wg_query_arg  arglist[2];

    /* Convert segid to string (TODO: can the db encode 64 bit values automatically?) */
    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_XEMEM_SEGMENT);    

    arglist[1].column = HDB_SEGMENT_SEGID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, segid);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    segment = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return segment;
}

static hdb_segment_t
__get_segment_by_name(hdb_db_t   db,
		      char     * name)
{
    hdb_segment_t segment = NULL;
    wg_query    * query   = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_XEMEM_SEGMENT);

    arglist[1].column = HDB_SEGMENT_NAME;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_str(db, name, NULL);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    segment = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return segment;
}

static int 
__create_segment_record(hdb_db_t        db,
			xemem_segid_t   segid,
			char          * name)
{
    void * hdr_rec        = NULL;
    void * rec            = NULL;
    int    segment_cnt    = 0;

    hdb_segment_t segment = NULL;



    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_XEMEM_HDR, NULL);
    
    if (!hdr_rec) {
        ERROR("malformed database. Missing xemem Header\n");
        return -1;
    }

    /* Ensure segid and name do not exist */
    rec = __get_segment_by_segid(db, segid);
    if (rec) {
        ERROR("xemem segment with segid %lli already present\n", segid);
        return -1;
    }

    if (name) {
	rec = __get_segment_by_name(db, name);
	if (rec) {
	    ERROR("xemem segment with name %s already present\n", name);
	    return -1;
	}
    }

    if (name == NULL) {
	name = "unnamed";
    }
    /* Insert segment into the db */
    rec = wg_create_record(db, 3);
    wg_set_field(db, rec, HDB_TYPE_FIELD,    wg_encode_int(db, HDB_REC_XEMEM_SEGMENT));
    wg_set_field(db, rec, HDB_SEGMENT_SEGID, wg_encode_int(db, segid));
    wg_set_field(db, rec, HDB_SEGMENT_NAME,  wg_encode_str(db, name, NULL));

    /* Update the xemem Header information */
    segment_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT));
    wg_set_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT, wg_encode_int(db, segment_cnt + 1));

    return 0;
}


int
hdb_create_xemem_segment(hdb_db_t      db,
			 xemem_segid_t segid,
			 char        * name)
{
    wg_int lock_id;
    int    ret;

    lock_id = wg_start_write(db);
    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    ret = __create_segment_record(db, segid, name);
    if (ret != 0) 
        ERROR("Could not create xemem database record\n");

out:
    if (wg_end_write(db, lock_id) == 0)
        ERROR("Apparently this is catastrophic...\n");

    return ret;
}


static int
__delete_segment(hdb_db_t        db,
		 xemem_segid_t   segid)
{
    void * hdr_rec        = NULL;
    int    segment_cnt    = 0;
    int    ret            = 0;

    hdb_segment_t segment = NULL;


    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_XEMEM_HDR, NULL);
    
    if (!hdr_rec) {
        ERROR("malformed database. Missing xemem Header\n");
        return -1;
    }

    /* Find record */

    segment = __get_segment_by_segid(db, segid);

    if (!segment) {
	ERROR("Could not find xemem segment (segid: %lli)\n", segid);
	return -1;
    }
    
    if (wg_delete_record(db, segment) != 0) {
        ERROR("Could not delete xemem segment from database\n");
        return ret;
    }

    /* Update the xemem Header information */
    segment_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT));
    wg_set_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT, wg_encode_int(db, segment_cnt - 1));

    return 0;
}



int
hdb_delete_xemem_segment(hdb_db_t      db,
			 xemem_segid_t segid)
{
    wg_int lock_id;
    int    ret;
    
    lock_id = wg_start_write(db);
    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    ret = __delete_segment(db, segid);
    if (ret != 0)
        ERROR("Could not delete xemem database record\n");

out:
    if (wg_end_write(db, lock_id) == 0)
        ERROR("Apparently this is catastrophic...\n");

    return ret;

}

static xemem_segid_t
__get_xemem_segid(hdb_db_t   db,
		  char     * name)
{
    hdb_segment_t segment = NULL;
    xemem_segid_t segid;

    segment = __get_segment_by_name(db, name);

    if (segment == NULL) {
	ERROR("Could not find XEMEM segment (name: %s)\n", name);
	return -1;
    }

    segid = wg_decode_int(db, wg_get_field(db, segment, HDB_SEGMENT_SEGID));
    
    return segid;
}


xemem_segid_t
hdb_get_xemem_segid(hdb_db_t   db,
		    char     * name)
{
    wg_int lock_id;
    xemem_segid_t segid;

    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    segid = __get_xemem_segid(db, name);

    if (!wg_end_read(db, lock_id))
        ERROR("Catastrophic database locking error\n");

    return segid;
}


static char *
__get_xemem_name(hdb_db_t       db,
		 xemem_segid_t segid)
{
    hdb_segment_t segment = NULL;
    char * name = NULL;

    segment = __get_segment_by_segid(db, segid);

    if (segment == NULL) {
	ERROR("Could not find XEMEM segment (id: %ll)\n", segid);
	return NULL;
    }

    name = wg_decode_str(db, wg_get_field(db, segment, HDB_SEGMENT_NAME));

    return name;
}


char * 
hdb_get_xemem_name(hdb_db_t      db,
		   xemem_segid_t segid)
{
    wg_int lock_id;
    char * name = NULL;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return NULL;
    }

    name = __get_xemem_name(db, segid);

    if (!wg_end_read(db, lock_id))
        ERROR("Catastrophic database locking error\n");

    return name;
}







static xemem_segid_t *
__get_segments(hdb_db_t   db,
	       int      * num_segments)
{
    void * hdr_rec = NULL;
    void * db_rec  = NULL;
    int    cnt     = 0;
    int    i       = 0;

    xemem_segid_t * segid_arr = NULL;
    hdb_segment_t   segment   = NULL;

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_XEMEM_HDR, NULL);    

    if (!hdr_rec) {
        ERROR("Malformed database. Missing xemem Header\n");
        return NULL;
    }

    cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT));

    segid_arr = calloc(sizeof(xemem_segid_t), cnt);

    for (i = 0; i < cnt; i++) {
        db_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_XEMEM_SEGMENT, db_rec);
        
        if (!db_rec) {
            ERROR("xemem Header state mismatch\n");
	    cnt = i;
            break;
        }

	segid_arr[i] = wg_decode_int(db, wg_get_field(db, db_rec, HDB_SEGMENT_SEGID));
    }

    *num_segments = cnt;
    return segid_arr;
}



xemem_segid_t *
hdb_get_segments(hdb_db_t db,
		 int    * num_segments)
{
    xemem_segid_t * segid_arr = NULL;
    wg_int lock_id;

    if (!num_segments) {
        return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return NULL;
    }

    segid_arr = __get_segments(db, num_segments);

    if (!wg_end_read(db, lock_id))
        ERROR("Catastrophic database locking error\n");

    return segid_arr;
}
