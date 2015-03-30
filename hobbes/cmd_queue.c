/* 
 * Command queue 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <dbapi.h>
#include <dballoc.h>

#include <pet_log.h>
#include <stdint.h>

#include "cmd_queue.h"
#include "xpmem.h"


#define CMD_QUEUE_SIZE (16 * 1024 * 1024)


/* Command Queue Row Types */
#define HCQ_HEADER_TYPE           0
#define HCQ_CMD_TYPE              1

/* Column Fields */

#define HCQ_TYPE_FIELD            0

/* Header */
#define HCQ_HDR_FIELD_NEXT_AVAIL  1
#define HCQ_HDR_FIELD_PENDING     2
#define HCQ_HDR_FIELD_OUTSTANDING 3

/* Commands */
#define HCQ_CMD_FIELD_ID          1
#define HCQ_CMD_FIELD_CMD_CODE    2
#define HCQ_CMD_FIELD_CMD_SIZE    3
#define HCQ_CMD_FIELD_CMD_DATA    4
#define HCQ_CMD_FIELD_STATUS      5
#define HCQ_CMD_FIELD_SEGID       6
#define HCQ_CMD_FIELD_RET_CODE    7
#define HCQ_CMD_FIELD_RET_SIZE    8
#define HCQ_CMD_FIELD_RET_DATA    9 


typedef enum {
    HCQ_INVALID = 0,
    HCQ_SERVER  = 1,
    HCQ_CLIENT  = 2} hcq_type_t;



struct cmd_queue {
    hcq_type_t type; 
    int fd;

    void  * db_addr;
    void  * db;

    xemem_segid_t segid;
    xemem_apid_t  apid;
};



static int
init_cmd_queue(struct cmd_queue * cq) 
{
    void * rec = NULL;
    void * db  = cq->db;

    /* Create Header */
    rec = wg_create_record(cq->db, 4);
    wg_set_field(db, rec, HCQ_TYPE_FIELD,             wg_encode_int(db, HCQ_HEADER_TYPE));
    wg_set_field(db, rec, HCQ_HDR_FIELD_NEXT_AVAIL,   wg_encode_int(db, 0));
    wg_set_field(db, rec, HCQ_HDR_FIELD_PENDING,      wg_encode_int(db, 0));
    wg_set_field(db, rec, HCQ_HDR_FIELD_OUTSTANDING,  wg_encode_int(db, 0));

    return 0;
}

static void *
get_db_addr(void * db) {
#ifdef USE_DATABASE_HANDLE
   return ((db_handle *)db)->db;
#else
    return db;
#endif
}


xemem_segid_t
hcq_get_segid(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    return cq->segid;
}

int
hcq_get_fd(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    return cq->fd;
}




hcq_handle_t
hcq_create_queue(char * name) 
{
    struct cmd_queue * cq    = NULL;
    xemem_segid_t      segid = 0;

    void * db_addr = NULL;
    void * db      = NULL;
    int    fd      = 0;


    db = wg_attach_local_database(CMD_QUEUE_SIZE);

    if (db == NULL) {
	ERROR("Could not create database\n");
	return NULL;
    }


    db_addr = get_db_addr(db);

    segid = xemem_make_signalled(db_addr, CMD_QUEUE_SIZE, 
				 XPMEM_PERMIT_MODE, (void *)0600,
				 NULL, &fd);

    if (segid <= 0) {
        ERROR("Could not register XEMEM segment for command queue\n");
	wg_delete_local_database(db);
	return NULL;
    }
    
    cq = calloc(sizeof(struct cmd_queue), 1);

    cq->type      = HCQ_SERVER;
    cq->fd        = fd;
    cq->db_addr   = db_addr;
    cq->db        = db;
    cq->segid     = segid;
    cq->apid      = 0;

    init_cmd_queue(cq);

    return cq;
}


void 
hcq_free_queue(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    wg_delete_local_database(cq->db);

    xemem_remove(cq->segid);

    free(cq);

    return;
}


hcq_handle_t
hcq_connect(xemem_segid_t segid)
{
    struct cmd_queue * cq      = NULL;
    xemem_apid_t       apid    = 0;
    void             * db_addr = NULL;
    void             * db      = NULL;


    struct xemem_addr addr;

    apid = xemem_get(segid, XEMEM_RDWR, XEMEM_PERMIT_MODE, NULL);
    
    if (apid <= 0) {
	ERROR("Failed to get APID for command queue\n");
	return cq;
    }

    addr.apid   = apid;
    addr.offset = 0;

    db_addr = xemem_attach(addr, CMD_QUEUE_SIZE, NULL);

    if (db_addr == MAP_FAILED) {
	ERROR("Failed to attach to command queue\n");
	xemem_release(apid);
	return NULL;
    }


    db = wg_attach_existing_local_database(db_addr);

    if (db == NULL) {
	ERROR("Failed to attach command queue database\n");
	xemem_detach(db_addr);
	xemem_release(apid);
	return NULL;
    }

    cq = calloc(sizeof(struct cmd_queue), 1);

    cq->type    = HCQ_CLIENT;
    cq->fd      = -1;
    cq->db_addr = db_addr;
    cq->db      = db;
    cq->segid   = segid;
    cq->apid    = apid;

    return cq;
}


void
hcq_disconnect(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    wg_detach_database(cq->db);

    xemem_detach(cq->db_addr);
    xemem_release(cq->apid);

    free(cq);

    return;
}

static void * 
__get_cmd_rec(struct cmd_queue * cq, 
	      hcq_cmd_t          cmd)
{
    void        * cmd_rec  = NULL;
    wg_query    * query    = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = HCQ_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(cq->db, HCQ_CMD_TYPE);    

    arglist[1].column = HCQ_CMD_FIELD_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(cq->db, cmd);

    query   = wg_make_query(cq->db, NULL, 0, arglist, 2);

    cmd_rec = wg_fetch(cq->db, query);

    wg_free_query(cq->db, query);
    wg_free_query_param(cq->db, arglist[0].value);
    wg_free_query_param(cq->db, arglist[1].value);

    return cmd_rec;

}




static hcq_cmd_t
__cmd_issue(struct cmd_queue * cq,
	    uint64_t           cmd_code,
	    uint32_t           data_size,
	    void             * data)
{
    void     * db      = cq->db;
    void     * hdr_rec = NULL;
    void     * cmd_rec = NULL;
    hcq_cmd_t  cmd_id  = HCQ_INVALID_CMD;
    uint64_t   cmd_cnt = 0;


    hdr_rec = wg_find_record_int(db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_HEADER_TYPE, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing Header\n");
	return HCQ_INVALID_CMD;
    }

    /* Get CMD ID */
    // grab + increment next_id
    cmd_id  = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_NEXT_AVAIL));
    cmd_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING));

    /* Create Command */
    cmd_rec = wg_create_record(db, 10);

    wg_set_field(db, cmd_rec, HCQ_TYPE_FIELD,             wg_encode_int(db, HCQ_CMD_TYPE));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_ID,           wg_encode_int(db, cmd_id));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_CODE,     wg_encode_int(db, cmd_code));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_SEGID,        wg_encode_int(db, 0));  
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_SIZE,     wg_encode_int(db, data_size));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_DATA,     wg_encode_blob(db, data, NULL, data_size)); 
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_STATUS,       wg_encode_int(db, HCQ_CMD_PENDING)); 

    /* Activate in queue */
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_NEXT_AVAIL,   wg_encode_int(db, cmd_id  + 1));
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING,  wg_encode_int(db, cmd_cnt + 1));

    return cmd_id;
}



hcq_cmd_t 
hcq_cmd_issue(hcq_handle_t hcq, 
	      uint64_t     cmd_code,
	      uint32_t     data_size,
	      void       * data)
{
    struct cmd_queue * cq = hcq;
    wg_int    lock_id;
    hcq_cmd_t cmd = HCQ_INVALID_CMD;

    lock_id = wg_start_write(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HCQ_INVALID_CMD;
    }

    cmd = __cmd_issue(cq, cmd_code, data_size, data);
    
    if (!wg_end_write(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HCQ_INVALID_CMD;
    }

    return cmd;
}


static hcq_cmd_status_t
__get_cmd_status(struct cmd_queue * cq, 
		 hcq_cmd_t          cmd)
{
    hcq_cmd_status_t status = -1;
    void * cmd_rec = NULL;


    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find command (ID=%ll) in queue\n", cmd);
	return -1;
    }
    
    status = wg_decode_int(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_STATUS));

    return status;
}


hcq_cmd_status_t
hcq_get_cmd_status(hcq_handle_t hcq, 
		   hcq_cmd_t    cmd)
{
    struct cmd_queue * cq     = hcq;
    hcq_cmd_status_t   status = -1;
    wg_int             lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    status = __get_cmd_status(cq, cmd);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return status; 
}

static int64_t
__get_ret_code(struct cmd_queue * cq, 
	       hcq_cmd_t          cmd)
{
    int64_t ret_code = -1;
    void  * cmd_rec  = NULL;
    

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find command (ID=%ll) in queue\n", cmd);
	return -1;
    }


    ret_code = wg_decode_int(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_RET_CODE));

    return ret_code;
}


int64_t
hcq_get_ret_code(hcq_handle_t hcq, 
		 hcq_cmd_t    cmd)
{
    struct cmd_queue * cq       = hcq;
    int64_t            ret_code = -1;
    wg_int             lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret_code = __get_ret_code(cq, cmd);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret_code; 
}


static void *
__get_ret_data(struct cmd_queue * cq, 
	       hcq_cmd_t          cmd,
	       uint32_t         * size)
{
    void    * cmd_rec   = NULL;
    void    * data      = NULL;
    uint32_t  data_size = 0;

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find command (ID=%ll) in queue\n", cmd);
	return NULL;
    }

    data_size = wg_decode_int(cq->db,  wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_RET_CODE));
    data      = wg_decode_blob(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_RET_DATA));

    *size = data_size;
    return data;
}


void *
hcq_get_ret_data(hcq_handle_t  hcq, 
		 hcq_cmd_t     cmd,
		 uint32_t    * size)
{
    struct cmd_queue * cq  = hcq;

    void     * data      = NULL;
    uint32_t   data_size = 0;
    wg_int     lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    data = __get_ret_data(cq, cmd, &data_size);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return NULL;
    }

    *size = data_size;

    return data; 
}


static int
__complete_cmd(struct cmd_queue * cq, 
	       hcq_cmd_t          cmd)
{
    void * cmd_rec = NULL;

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find Command (ID=%llu) in queue\n", cmd);
	return -1;
    }

    if (wg_delete_record(cq->db, cmd_rec) != 0) {
	ERROR("Could not delete completed Command from queue\n");
	return -1;
    }

    return 0;
}

int 
hcq_cmd_complete(hcq_handle_t hcq,
		 hcq_cmd_t    cmd)
{
    struct cmd_queue * cq  = hcq;
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __complete_cmd(cq, cmd);
    
    if (!wg_end_write(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret; 
}


static hcq_cmd_t
__get_next_cmd(struct cmd_queue * cq)
{
    hcq_cmd_t   next_cmd = HCQ_INVALID_CMD;
    void      * hdr_rec  = NULL;
    
    hdr_rec = wg_find_record_int(cq->db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_HEADER_TYPE, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing  Header\n");
	return HCQ_INVALID_CMD;
    }

    next_cmd = wg_decode_int(cq->db, wg_get_field(cq->db, hdr_rec, HCQ_HDR_FIELD_PENDING));

    return next_cmd;
}

hcq_cmd_t
hcq_get_next_cmd(hcq_handle_t hcq)
{
    struct cmd_queue * cq  = hcq;

    hcq_cmd_t  cmd  = HCQ_INVALID_CMD;
    wg_int     lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HCQ_INVALID_CMD;
    }

    cmd = __get_next_cmd(cq);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HCQ_INVALID_CMD;
    }

    return cmd; 
}


static uint64_t 
__get_cmd_code(struct cmd_queue * cq,
	       hcq_cmd_t          cmd)
{
    void     * cmd_rec  = NULL;
    uint64_t   cmd_code = 0;

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find command (ID=%ll) in queue\n", cmd);
	return 0;
    }
    

    cmd_code = wg_decode_int(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_CMD_CODE));

    return cmd_code;
}


uint64_t
hcq_get_cmd_code(hcq_handle_t hcq,
		 hcq_cmd_t    cmd)
{
    struct cmd_queue * cq  = hcq;
    uint64_t   cmd_code = 0;
    wg_int     lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return 0;
    }

    cmd_code = __get_cmd_code(cq, cmd);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return 0;
    }

    return cmd_code; 
}


static void *
__get_cmd_data(struct cmd_queue * cq, 
	       hcq_cmd_t          cmd,
	       uint32_t         * size)
{
    void    * cmd_rec   = NULL;
    void    * data      = NULL;
    uint32_t  data_size = 0;

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (cmd_rec == NULL) {
	ERROR("Could not find command (ID=%ll) in queue\n", cmd);
	return NULL;
    }

    data_size = wg_decode_int(cq->db,  wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_CMD_CODE));
    data      = wg_decode_blob(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_CMD_DATA));

    *size = data_size;
    return data;
}


void *
hcq_get_cmd_data(hcq_handle_t  hcq, 
		 hcq_cmd_t     cmd,
		 uint32_t    * size)
{
    struct cmd_queue * cq  = hcq;

    void     * data      = NULL;
    uint32_t   data_size = 0;
    wg_int     lock_id;

    lock_id = wg_start_read(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    data = __get_cmd_data(cq, cmd, &data_size);
    
    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return NULL;
    }

    *size = data_size;

    return data; 
}

static int
__cmd_return(struct cmd_queue * cq, 
	     hcq_cmd_t          cmd, 
	     int64_t            ret_code, 
	     uint32_t           data_size,
	     void             * data)
{
    void     * db      = cq->db;
    void     * hdr_rec = NULL;
    void     * cmd_rec = NULL;
    hcq_cmd_t  pending = HCQ_INVALID_CMD;
    uint64_t   cmd_cnt = 0;

    
    hdr_rec = wg_find_record_int(db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_HEADER_TYPE, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing Header\n");
	return -1;
    }

    pending = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_PENDING));
    cmd_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING));

    if (cmd_cnt <= 0) {
	ERROR("Command count is <= 0\n");
	return -1;
    }

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (!cmd_rec) {
	ERROR("Could not find command to return (ID=%llu)\n", cmd);
	return -1;
    }

    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_CODE, wg_encode_int(db, ret_code));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_SIZE, wg_encode_int(db, data_size));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_DATA, wg_encode_blob(db, data, NULL, data_size));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_STATUS,   wg_encode_int(db, HCQ_CMD_RETURNED));

    /* Update the header fields */
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING, wg_encode_int(db, cmd_cnt - 1));
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_PENDING,     wg_encode_int(db, pending + 1));

    /* Signal Client segid */


    return 0;
}	     


int
hcq_cmd_return(hcq_handle_t hcq, 
	       hcq_cmd_t    cmd, 
	       int64_t      ret_code, 
	       uint32_t     data_size,
	       void       * data)
{
    
    struct cmd_queue * cq = hcq;
    wg_int  lock_id;
    int     ret = 0;

    lock_id = wg_start_write(cq->db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __cmd_return(cq, cmd, ret_code, data_size, data);
    
    if (!wg_end_write(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret;
}
