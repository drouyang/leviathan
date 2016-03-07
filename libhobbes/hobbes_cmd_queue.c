/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */



#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <poll.h>
#include <stdint.h>

#include <dbapi.h>
#include <dballoc.h>

#include <pet_log.h>
#include <pet_hashtable.h>


#include "hobbes_cmd_queue.h"
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


struct server_cmd_queue {
    /* segid mapping the hcq (created by server/attached by client) */
    xemem_segid_t      segid;

    /* fd to poll for incoming commands */
    int                fd;    

    /* hashtable of command handlers */
    struct hashtable * cmd_handlers;

    /* hashtable of client connections 
     * (maps client segid to server-attached apid)
     */
    struct hashtable * connections;
};

struct client_cmd_queue {
    /* apid to attach/kick server */
    xemem_apid_t       apid;

    /* segid used to kick client upon command completion */
    xemem_segid_t      segid;

    /* fd to poll for command responses */
    int                fd;
};


struct cmd_queue {
    hcq_type_t type; 

    /* Database memory mappings */
    void  * db_addr;
    void  * db;

    union {
	struct server_cmd_queue server;
	struct client_cmd_queue client;
    };
};


static uint32_t
handler_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
handler_eq_fn(uintptr_t key1,
	      uintptr_t key2)
{
    return (key1 == key2);
}




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

    if (cq->type != HCQ_SERVER)
	return XEMEM_INVALID_SEGID;

    return cq->server.segid;
}

int
hcq_get_fd(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    if (cq->type == HCQ_SERVER)
	return cq->server.fd;
    else if (cq->type == HCQ_CLIENT)
	return cq->client.fd;
    else
	return -1;
}

hcq_handle_t
hcq_create_queue(char * name) 
{
    struct cmd_queue * cq     = NULL;
    struct hashtable * cmd_ht = NULL;
    struct hashtable * seg_ht = NULL;
    xemem_segid_t      segid  = 0;

    void * db_addr = NULL;
    void * db      = NULL;
    int    fd      = 0;


    db = wg_attach_local_database(CMD_QUEUE_SIZE);

    if (db == NULL) {
	ERROR("Could not create database\n");
	return HCQ_INVALID_HANDLE;
    }


    db_addr = get_db_addr(db);

    /* Create the signallable segid */
    segid = xemem_make_signalled(db_addr, CMD_QUEUE_SIZE,
				 name, &fd);

    if (segid <= 0) {
        ERROR("Could not register XEMEM segment for command queue\n");
	goto segid_out;
    }

    /* Create the htable of handlers */
    cmd_ht = pet_create_htable(0, handler_hash_fn, handler_eq_fn);

    if (cmd_ht == NULL) {
	ERROR("Could not create hcq command hashtable\n");
	goto cmd_ht_out;
    }

    /* Create the htable of segids */
    seg_ht = pet_create_htable(0, handler_hash_fn, handler_eq_fn);

    if (seg_ht == NULL) {
	ERROR("Could not create hcq connection hashtable\n");
	goto seg_ht_out;
    }
    
    cq = calloc(sizeof(struct cmd_queue), 1);
    if (!cq) {
	ERROR("Could not calloc command queue\n");
	goto cq_out;
    }

    cq->type    = HCQ_SERVER;
    cq->db_addr = db_addr;
    cq->db      = db;

    cq->server.segid        = segid;
    cq->server.fd	    = fd;
    cq->server.cmd_handlers = cmd_ht;
    cq->server.connections  = seg_ht;

    init_cmd_queue(cq);

    return cq;

cq_out:
    pet_free_htable(seg_ht, 0, 0);

seg_ht_out:
    pet_free_htable(cmd_ht, 0, 0);

cmd_ht_out:
    xemem_remove(segid);

segid_out:
    wg_delete_local_database(db);

    return HCQ_INVALID_HANDLE;
}


void 
hcq_free_queue(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    if (cq->type != HCQ_SERVER) {
	ERROR("Only server can free HCQs\n");
	return;
    }

    wg_delete_local_database(cq->db);

    close(cq->server.fd);
    xemem_remove(cq->server.segid);

    /* Remove all apids from the connection table */
    {
	struct hashtable_iter * iter = NULL;

	iter = pet_htable_create_iter(cq->server.connections);
	if (iter == NULL) {
	    ERROR("Could not create connections table iterator\n");
	    goto out;
	}

	do {
	    xemem_apid_t apid = (xemem_apid_t)pet_htable_get_iter_value(iter);
	    xemem_release(apid);
	} while (pet_htable_iter_remove(iter, 0) != 0);

	pet_htable_free_iter(iter);
    }

out:
    pet_free_htable(cq->server.connections, 0, 0);

    pet_free_htable(cq->server.cmd_handlers, 0, 0);

    free(cq);

    return;
}

int
hcq_register_cmd(hcq_handle_t hcq,
		 uint64_t     cmd_code,
		 hcq_cmd_fn   handler_fn)
{
    struct cmd_queue * cq = hcq;

    if (cq->type != HCQ_SERVER) {
	ERROR("Only the server can register an HCQ command\n");
	return -1;
    }

    if (pet_htable_search(cq->server.cmd_handlers, cmd_code) != 0) {
	ERROR("Attempted to register duplicate command handler (cmd=%lu)\n", cmd_code);
	return -1;
    }

    if (pet_htable_insert(cq->server.cmd_handlers, cmd_code, (uintptr_t)handler_fn) == 0) {
	ERROR("Could not register hcq command (cmd=%lu)\n", cmd_code);
	return -1;
    }

    return 0;
}

hcq_cmd_fn
hcq_get_cmd_handler(hcq_handle_t hcq,
		    hcq_cmd_t    cmd)
{
    struct cmd_queue * cq = hcq;

    if (cq->type != HCQ_SERVER) {
	ERROR("Only the server can access command handlers\n");
	return NULL;
    }

    return (hcq_cmd_fn)pet_htable_search(cq->server.cmd_handlers, (uintptr_t)hcq_get_cmd_code(hcq, cmd));
}

hcq_handle_t
hcq_connect(xemem_segid_t segid)
{
    struct cmd_queue * cq      = NULL;
    xemem_apid_t       apid    = 0;
    void             * db_addr = NULL;
    void             * db      = NULL;

    xemem_segid_t client_segid = 0;
    int           client_fd    = 0;

    struct xemem_addr addr;
    
//    printf("HCQ SEGID = %ld\n", segid);

    client_segid = xemem_make_signalled(NULL, 0, NULL, &client_fd);
    
    if (client_segid <= 0) {
	ERROR("Could not create client signal segid\n");
	goto out_xemem_make;
    }

    apid = xemem_get(segid, XEMEM_RDWR);
    
    if (apid <= 0) {
	ERROR("Failed to get APID for command queue\n");
	goto out_xemem_get;
    }

    addr.apid   = apid;
    addr.offset = 0;

    db_addr = xemem_attach(addr, CMD_QUEUE_SIZE, NULL);

    if (db_addr == MAP_FAILED) {
	ERROR("Failed to attach to command queue\n");
	goto out_xemem_attach;
    }


    db = wg_attach_existing_local_database(db_addr);

    if (db == NULL) {
	ERROR("Failed to attach command queue database\n");
	goto out_db_attach;
    }


    cq = calloc(sizeof(struct cmd_queue), 1);

    cq->type    = HCQ_CLIENT;
    cq->db_addr = db_addr;
    cq->db      = db;

    cq->client.fd    = client_fd;
    cq->client.apid  = apid;
    cq->client.segid = client_segid;

    return cq;

out_db_attach:
    xemem_detach(db_addr);

out_xemem_attach:
    xemem_release(apid);

out_xemem_get:
    xemem_remove(client_segid);

out_xemem_make:
    return HCQ_INVALID_HANDLE;
}


void
hcq_disconnect(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;

    if (cq->type != HCQ_CLIENT) {
	ERROR("Only clients can disconnect an HCQ\n");
	return;
    }

    wg_detach_local_database(cq->db);

    xemem_detach(cq->db_addr);

    xemem_release(cq->client.apid);

    close(cq->client.fd);
    xemem_remove(cq->client.segid);

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

    if ((data_size > 0) && (data == NULL)) {
	ERROR("NULL data pointer, but positive data size\n");
	return HCQ_INVALID_CMD;
    }


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
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_SEGID,        wg_encode_int(db, cq->client.segid));  
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_SIZE,     wg_encode_int(db, data_size));

    if (data_size > 0) {
	wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_DATA,     wg_encode_blob(db, data, NULL, data_size)); 
    }

    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_STATUS,       wg_encode_int(db, HCQ_CMD_PENDING)); 

    /* Activate in queue */
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_NEXT_AVAIL,   wg_encode_int(db, cmd_id  + 1));
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING,  wg_encode_int(db, cmd_cnt + 1));


    //printf("Signalling apid %lu\n", cq->apid);

    xemem_signal(cq->client.apid);

    return cmd_id;
}



hcq_cmd_t 
hcq_cmd_issue(hcq_handle_t hcq, 
	      uint64_t     cmd_code,
	      uint32_t     data_size,
	      void       * data)
{
    struct cmd_queue * cq  = hcq;
    hcq_cmd_t          cmd = HCQ_INVALID_CMD;

    wg_int    lock_id;

    if (cq->type != HCQ_CLIENT) {
	ERROR("Only clients can issue HCQ commands\n");
	return HCQ_INVALID_CMD;
    }

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
    
    /* poll for completion */
    {
	struct pollfd ufd = {cq->client.fd, POLLIN, 0};

	while (hcq_get_cmd_status(hcq, cmd) != HCQ_CMD_RETURNED) {
	    if (poll(&ufd, 1, -1) == -1) { 
		ERROR("poll() error\n");
	    } else {
		xemem_ack(cq->client.fd);
	    }
	}
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
	ERROR("Could not find command (ID=%lu) in queue\n", cmd);
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
	ERROR("Could not find command (ID=%lu) in queue\n", cmd);
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
	ERROR("Could not find command (ID=%lu) in queue\n", cmd);
	return NULL;
    }

    data_size = wg_decode_int(cq->db,  wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_RET_SIZE));

    if (data_size > 0) {
	data  = wg_decode_blob(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_RET_DATA));
    }

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
	ERROR("Could not find Command (ID=%lu) in queue\n", cmd);
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
    uint64_t    cmd_cnt  = 0;
    void      * hdr_rec  = NULL;
    void      * db       = cq->db;
    
    hdr_rec = wg_find_record_int(db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_HEADER_TYPE, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing  Header\n");
	return HCQ_INVALID_CMD;
    }

    cmd_cnt  = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING));
    next_cmd = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_PENDING));

    if (cmd_cnt <= 0) {
	return HCQ_INVALID_CMD;
    }

    /* Mark command as taken */
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING, wg_encode_int(db, cmd_cnt - 1));
    wg_set_field(db, hdr_rec, HCQ_HDR_FIELD_PENDING,     wg_encode_int(db, next_cmd + 1));

    /* Quiesce the signal */
    xemem_ack(cq->server.fd);

    return next_cmd;
}

hcq_cmd_t
hcq_get_next_cmd(hcq_handle_t hcq)
{
    struct cmd_queue * cq  = hcq;

    hcq_cmd_t  cmd  = HCQ_INVALID_CMD;
    wg_int     lock_id;

    if (cq->type != HCQ_SERVER) {
	ERROR("Only server can retrieve HCQ commands\n");
	return HCQ_INVALID_CMD;
    }

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
	ERROR("Could not find command (ID=%lu) in queue\n", cmd);
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
	ERROR("Could not find command (ID=%lu) in queue\n", cmd);
	return NULL;
    }

    data_size = wg_decode_int(cq->db,  wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_CMD_SIZE));

    if (data_size > 0) {
	data  = wg_decode_blob(cq->db, wg_get_field(cq->db, cmd_rec, HCQ_CMD_FIELD_CMD_DATA));
    }

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
    void        * db      = cq->db;
    void        * cmd_rec = NULL;
    
    if ((data_size > 0) && (data == NULL)) {
	ERROR("NULL Data pointer, but positive data size\n");
	return -1;
    }

    cmd_rec = __get_cmd_rec(cq, cmd);

    if (!cmd_rec) {
	ERROR("Could not find command to return (ID=%lu)\n", cmd);
	return -1;
    }

    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_CODE, wg_encode_int(db, ret_code));
    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_SIZE, wg_encode_int(db, data_size));

    if (data_size > 0) {
	wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_RET_DATA, wg_encode_blob(db, data, NULL, data_size));
    }

    wg_set_field(db, cmd_rec, HCQ_CMD_FIELD_STATUS,   wg_encode_int(db, HCQ_CMD_RETURNED));


    /* Signal Client apid */
    {
	xemem_segid_t segid  = 0;
	xemem_apid_t  apid   = 0; 

	segid = wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_SEGID));

	if (segid == 0) 
	    return 0;

	/* Query connections table for apid */
	apid = (xemem_apid_t)pet_htable_search(cq->server.connections, (uintptr_t)segid);
	if (apid == 0) { 
	    /* Look it up now */
	    apid = xemem_get(segid, XEMEM_RDWR);
	    if (apid == -1) {
		ERROR("Could not find apid for HCQ client: cannot kick client\n");
		return 0;
	    } 

	    /* Remember it for future commands issued from this client */
	    if (pet_htable_insert(cq->server.connections, (uintptr_t)segid, (uintptr_t)apid) == 0) {
		ERROR("Could not update connections hashtable. This may result in a large "
			"performance penalty\n");
	    }
	}

	xemem_signal(apid);
    }

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

    if (cq->type != HCQ_SERVER) {
	ERROR("Only server can return an HCQ command\n");
	return -1;
    }

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

void
__dump_queue(struct cmd_queue * cq)
{
    void        * db      = cq->db;
    void        * hdr_rec = NULL;
    void        * cmd_rec = NULL;

    hcq_cmd_t     pending = HCQ_INVALID_CMD;
    uint64_t      cmd_cnt = 0;

    hdr_rec = wg_find_record_int(db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_HEADER_TYPE, NULL);

    if (!hdr_rec) {
	ERROR("Malformed database: Missing Header field\n");
    }

    pending = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_PENDING));
    cmd_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HCQ_HDR_FIELD_OUTSTANDING));

    printf("HCQ -- Command Count: %lu ;  Pending Command: %lu\n", 
	   cmd_cnt, pending);


    while (1) {
	cmd_rec = wg_find_record_int(db, HCQ_TYPE_FIELD, WG_COND_EQUAL, HCQ_CMD_TYPE, cmd_rec);

	if (!cmd_rec) {
	    break;
	}

	printf("CMD %lu: CODE=%lu, SIZE=%lu, STATUS=%lu, SEGID=%lu,  RET_CODE=%lu, RET_SIZE=%lu\n",
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_ID)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_CODE)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_CMD_SIZE)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_STATUS)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_SEGID)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_RET_CODE)),
	       wg_decode_int(db, wg_get_field(db, cmd_rec, HCQ_CMD_FIELD_RET_SIZE)));

    } 

    return;
}


void
hcq_dump_queue(hcq_handle_t hcq)
{
    struct cmd_queue * cq = hcq;
    wg_int lock_id;

    lock_id = wg_start_read(cq->db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return;
    }

    __dump_queue(cq);

    if (!wg_end_read(cq->db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return;
    }

    return;
}
