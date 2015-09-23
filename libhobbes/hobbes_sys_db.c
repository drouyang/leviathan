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
#include <stdint.h>

#include <dbapi.h>
#include <dballoc.h>

#include <pet_log.h>

#include "hobbes.h"
#include "hobbes_db.h"
#include "hobbes_db_schema.h"


static void *
__get_sys_hdr(hdb_db_t db)
{
    static void * sys_hdr = NULL;
    
    if (sys_hdr == NULL) {
	sys_hdr = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);
    }

    return sys_hdr;
}


/*
 * System Info Database records
 */

static int
__init_system_info(hdb_db_t db,
		   uint32_t numa_nodes,
		   uint64_t mem_blk_size)
{
    void * rec = __get_sys_hdr(db);

    if (rec) {
	ERROR("Tried to initialize already present system header\n");
	return -1;
    }

    /* Create System Info Header */
    rec = wg_create_record(db, 8);
    wg_set_field(db, rec, HDB_TYPE_FIELD,               wg_encode_int(db, HDB_REC_SYS_HDR));
    wg_set_field(db, rec, HDB_SYS_HDR_CPU_CNT,          wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_NUMA_CNT,         wg_encode_int(db, numa_nodes));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_BLK_SIZE,     wg_encode_int(db, mem_blk_size));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_BLK_CNT,      wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_FREE_BLK_CNT, wg_encode_int(db, 0));

    wg_set_field(db, rec, HDB_SYS_HDR_MEM_FREE_LIST,    wg_encode_null(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_BLK_LIST,     wg_encode_null(db, 0));
 
    return 0;
}

int
hdb_init_system_info(hdb_db_t db,
		     uint32_t numa_nodes,
		     uint64_t mem_blk_size)
{
    wg_int   lock_id;
    int      ret      = -1;
    
    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    ret = __init_system_info(db, numa_nodes, mem_blk_size);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}

static uint32_t
__get_sys_numa_cnt(hdb_db_t db)
{
    uint32_t   numa_cnt = 0;
    void     * hdr_rec  = __get_sys_hdr(db);
    
    if (!hdr_rec) {
	ERROR("Malformed database. Missing System Info Header\n");
	return -1;
    }
    
    numa_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_NUMA_CNT));
    
    return numa_cnt;
}




uint32_t
hdb_get_sys_numa_cnt(hdb_db_t db) 
{
    uint32_t   numa_cnt = 0;
    wg_int     lock_id;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    numa_cnt = __get_sys_numa_cnt(db);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return numa_cnt;
}

static uint64_t 
__get_sys_blk_size(hdb_db_t db)
{
    void     * hdr_rec  = NULL;
    uint64_t   blk_size = 0;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed database. Missing System Info Header\n");
	return -1;
    }
    
    blk_size = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_SIZE));
    
    return blk_size;
}

uint64_t 
hdb_get_sys_blk_size(hdb_db_t db)
{
    uint64_t   blk_size = 0;
    wg_int     lock_id;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    blk_size = __get_sys_blk_size(db);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return blk_size;
}


static uint64_t 
__get_sys_blk_cnt(hdb_db_t db)
{
    void     * hdr_rec  = NULL;
    uint64_t   blk_cnt = 0;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed database. Missing System Info Header\n");
	return -1;
    }
    
    blk_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_CNT));
    
    return blk_cnt;
}

uint64_t 
hdb_get_sys_blk_cnt(hdb_db_t db)
{
    uint64_t   blk_cnt = 0;
    wg_int     lock_id;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    blk_cnt = __get_sys_blk_cnt(db);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return blk_cnt;
}


static uint64_t 
__get_sys_free_blk_cnt(hdb_db_t db)
{
    void     * hdr_rec  = NULL;
    uint64_t   blk_cnt = 0;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed database. Missing System Info Header\n");
	return -1;
    }
    
    blk_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_FREE_BLK_CNT));
    
    return blk_cnt;
}

uint64_t 
hdb_get_sys_free_blk_cnt(hdb_db_t db)
{
    uint64_t   blk_cnt = 0;
    wg_int     lock_id;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    blk_cnt = __get_sys_free_blk_cnt(db);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return blk_cnt;
}






/* CPU Record Accessors */

static hdb_cpu_t 
__get_cpu_by_id(hdb_db_t db,
		uint32_t cpu_id)
{
    hdb_cpu_t      cpu   = NULL;
    wg_query     * query = NULL;
    wg_query_arg   arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_CPU);

    arglist[1].column = HDB_CPU_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, cpu_id);

    query = wg_make_query(db, NULL, 0, arglist, 2);
    
    cpu   = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return cpu;
}


static int
__register_cpu(hdb_db_t    db,
	       uint32_t    cpu_id,
	       uint32_t    numa_node,
	       cpu_state_t state)
{
    uint32_t   cpu_cnt  = 0;
    hdb_cpu_t  cpu      = NULL;
    void     * hdr_rec  = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }
    
    if (__get_cpu_by_id(db, cpu_id)) {
	ERROR("Tried to register a CPU (%u) that is already present\n", cpu_id);
	return -1;
    }
    
    cpu_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_CPU_CNT));


    cpu = wg_create_record(db, 5);
    wg_set_field(db, cpu, HDB_TYPE_FIELD,      wg_encode_int(db, HDB_REC_CPU));
    wg_set_field(db, cpu, HDB_CPU_ID,          wg_encode_int(db, cpu_id));
    wg_set_field(db, cpu, HDB_CPU_NUMA_NODE,   wg_encode_int(db, numa_node));
    wg_set_field(db, cpu, HDB_CPU_STATE,       wg_encode_int(db, state));
    wg_set_field(db, cpu, HDB_CPU_ENCLAVE_ID,  wg_encode_int(db, HOBBES_INVALID_ID));

    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, HDB_SYS_HDR_CPU_CNT, wg_encode_int(db, cpu_cnt + 1));

    return 0;
}

int 
hdb_register_cpu(hdb_db_t    db,
		 uint32_t    cpu_id,
		 uint32_t    numa_node,
		 cpu_state_t state)
{
    wg_int   lock_id;
    int      ret      = -1;
    
    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __register_cpu(db, cpu_id, numa_node, state);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;

}


static hobbes_id_t
__get_cpu_enclave_id(hdb_db_t db,
		     uint32_t cpu_id)
{
    hdb_cpu_t   cpu        = __get_cpu_by_id(db, cpu_id);
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    if (!cpu) {
	ERROR("Could not find CPU (%u)\b", cpu_id);
	return HOBBES_INVALID_ID;
    }

    enclave_id = wg_decode_int(db, wg_get_field(db, cpu, HDB_CPU_ENCLAVE_ID));

    return enclave_id;
}


hobbes_id_t
hdb_get_cpu_enclave_id(hdb_db_t db,
		       uint32_t cpu_id)
{
    wg_int      lock_id;
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;
  
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    enclave_id = __get_cpu_enclave_id(db, cpu_id);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_ID;
    }

    return enclave_id;
}


static cpu_state_t
__get_cpu_state(hdb_db_t db,
		uint32_t cpu_id)
{
    hdb_cpu_t cpu     = __get_cpu_by_id(db, cpu_id);
    mem_state_t state = CPU_INVALID;

    if (!cpu) {
	ERROR("Could not find CPU (%u)\b", cpu_id);
	return CPU_INVALID;
    }

    state = wg_decode_int(db, wg_get_field(db, cpu, HDB_CPU_STATE));

    return state;
}


cpu_state_t
hdb_get_cpu_state(hdb_db_t db,
		  uint32_t cpu_id)
{
    wg_int      lock_id;
    cpu_state_t state = CPU_INVALID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return CPU_INVALID;
    }

    state = __get_cpu_state(db, cpu_id);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return CPU_INVALID;
    }

    return state;
}


static uint32_t
__get_cpu_numa_node(hdb_db_t db,
		    uint32_t cpu_id)
{
    hdb_cpu_t cpu        = __get_cpu_by_id(db, cpu_id);
    uint32_t  numa_node  = HOBBES_INVALID_NUMA_ID;

    if (!cpu) {
	ERROR("Could not find CPU (%u)\n", cpu_id);
	return HOBBES_INVALID_NUMA_ID;
    }

    numa_node = wg_decode_int(db, wg_get_field(db, cpu, HDB_CPU_NUMA_NODE));

    return numa_node;
}

uint32_t 
hdb_get_cpu_numa_node(hdb_db_t db,
		      uint32_t cpu_id)
{
    uint32_t numa_node = 0;
    wg_int   lock_id;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_NUMA_ID;
    }

    numa_node = __get_cpu_numa_node(db, cpu_id);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_NUMA_ID;
    }

    return numa_node;
}



static uint32_t *
__get_cpus(hdb_db_t   db, 
	   uint32_t * num_cpus)
{
    uint32_t * cpu_arr = NULL;
    void     * db_rec  = NULL;
    void     * hdr_rec = NULL;
    int        cnt     = 0;
    int        i       = 0;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed database. Missing System info header\n");
	return NULL;
    }

    cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_CPU_CNT));

    if (cnt == 0) {
	*num_cpus = 0;
	return NULL;
    }

    cpu_arr = calloc(sizeof(uint32_t), cnt);

    for (i = 0; i < cnt; i++) {
	db_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_CPU, db_rec);

	if (!db_rec) {
	    ERROR("System Info Header state mismatch\n");
	    cnt = i;
	    break;
	}

	cpu_arr[i] = wg_decode_int(db, wg_get_field(db, db_rec, HDB_CPU_ID));
    }

    *num_cpus = cnt;

    return cpu_arr;


}


uint32_t * 
hdb_get_cpus(hdb_db_t   db,
	     uint32_t * num_cpus)
{
    uint32_t * cpu_arr = NULL;
    wg_int     lock_id;

    if (!num_cpus) {
	return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    cpu_arr = __get_cpus(db, num_cpus);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return cpu_arr;
}



/* Memory resource Accessors */


static hdb_mem_t
__get_mem_blk_by_addr(hdb_db_t  db,
		      uintptr_t addr)
{
    hdb_mem_t      blk   = NULL;
    wg_query     * query = NULL;
    wg_query_arg   arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_MEM);

    arglist[1].column = HDB_MEM_BASE_ADDR;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, addr);

    query = wg_make_query(db, NULL, 0, arglist, 2);
    
    blk   = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return blk;
}

static uintptr_t
__get_mem_blk_addr(hdb_db_t  db,
		   hdb_mem_t blk)
{
    return wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_BASE_ADDR));
}


static uint32_t 
__get_mem_blk_numa(hdb_db_t  db, 
		   hdb_mem_t blk)
{
    return wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_NUMA_NODE));
}

static int
__remove_free_mem_blk(hdb_db_t   db,
		      void     * hdr,
		      hdb_mem_t  blk)
{
    wg_int next_fld = wg_get_field(db, blk, HDB_MEM_NEXT_FREE);
    wg_int prev_fld = wg_get_field(db, blk, HDB_MEM_PREV_FREE);

    wg_int hdr_fld  = wg_get_field(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST);

    if (wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_STATE)) != MEMORY_FREE) {
	ERROR("Tried to remove a non-free memory block from free list\n");
	return -1;
    }

    /* Update the head of the list if necessary */
    

    if ((hdr_fld != 0) && 
	(wg_decode_record(db, hdr_fld) == blk)) {
	wg_set_field(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST, next_fld);
    }

    if (next_fld) {
	hdb_mem_t next_blk = wg_decode_record(db, next_fld);
	wg_set_field(db, next_blk, HDB_MEM_PREV_FREE, prev_fld);
    }

    if (prev_fld) {
	hdb_mem_t prev_blk = wg_decode_record(db, prev_fld);
	wg_set_field(db, prev_blk, HDB_MEM_NEXT_FREE, next_fld);
    }

    wg_set_field(db, blk, HDB_MEM_NEXT_FREE, wg_encode_null(db, 0));
    wg_set_field(db, blk, HDB_MEM_PREV_FREE, wg_encode_null(db, 0));
    
    return 0;
}



static int 
__insert_free_mem_blk(hdb_db_t   db, 
		      void     * hdr,
		      hdb_mem_t  blk)
{
    uintptr_t blk_addr = __get_mem_blk_addr(db, blk);
    wg_int    iter_fld = wg_get_field(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST);

    if (iter_fld == 0) {
	/* First entry: Prev=NULL, Next=NULL, HDR=this_blk */
	    
	wg_set_field(db, blk, HDB_MEM_NEXT_FREE,         wg_encode_null   ( db, 0   ));
	wg_set_field(db, blk, HDB_MEM_PREV_FREE,         wg_encode_null   ( db, 0   ));
	wg_set_field(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST, wg_encode_record ( db, blk ));
	    
    } else if (blk_addr < __get_mem_blk_addr(db, wg_decode_record(db, iter_fld))) {
	/* Add to the head of the list: 
	 *   Prev=NULL, Next=iter_blk, HDR=this_blk, iter_blk.prev=this_blk 
	 */
	hdb_mem_t iter_blk = wg_decode_record(db, iter_fld);

	wg_set_field(db, blk,      HDB_MEM_PREV_FREE,          wg_encode_null   ( db, 0        ));
	wg_set_field(db, blk,      HDB_MEM_NEXT_FREE,          wg_encode_record ( db, iter_blk ));
	wg_set_field(db, iter_blk, HDB_MEM_PREV_FREE,          wg_encode_record ( db, blk      ));
	wg_set_field(db, hdr,      HDB_SYS_HDR_MEM_FREE_LIST,  wg_encode_record ( db, blk      ));

    } else {
	hdb_mem_t iter_blk = wg_decode_record(db, iter_fld);
	hdb_mem_t next_blk = NULL;
	wg_int    next_fld = NULL;

	while (1) {
	    uintptr_t next_addr = 0;

	    next_fld = wg_get_field(db, iter_blk, HDB_MEM_NEXT_FREE);
		
	    if (next_fld == 0) {
		break;
	    }

	    next_blk  = wg_decode_record(db, next_fld);
	    next_addr = __get_mem_blk_addr(db, next_blk);
		
	    if (next_addr > blk_addr) {
		break;
	    }
		
	    iter_blk = next_blk;
	}
	
	    
	/* At this point iter_blk must be valid, but next_blk may or may not be NULL 
	 *  Prev = iter_blk, next = next_blk, iter_blk.next = this_blk, if(next_blk) next_blk.prev=this_blk
	 */

	wg_set_field(db, blk,      HDB_MEM_PREV_FREE, wg_encode_record(db, iter_blk));
	wg_set_field(db, iter_blk, HDB_MEM_NEXT_FREE, wg_encode_record(db, blk));

	if (next_fld) {
	    wg_set_field(db, blk,      HDB_MEM_NEXT_FREE, wg_encode_record(db, next_blk));
	    wg_set_field(db, next_blk, HDB_MEM_PREV_FREE, wg_encode_record(db, blk));
	} else {
	    wg_set_field(db, blk,      HDB_MEM_NEXT_FREE, wg_encode_null(db, 0));
	}
    }

    {
	uint64_t free_mem = wg_decode_int(db, wg_get_field(db, hdr, HDB_SYS_HDR_MEM_FREE_BLK_CNT));
	wg_set_field(db, hdr, HDB_SYS_HDR_MEM_FREE_BLK_CNT, wg_encode_int(db, free_mem + 1));
	
    }

    return 0;	
}

static int 
__insert_mem_blk(hdb_db_t   db,
		 void     * hdr,
		 hdb_mem_t  blk)
{
    uint64_t  blk_cnt  = 0;
    uintptr_t blk_addr = __get_mem_blk_addr(db, blk);

    blk_cnt = wg_decode_int(db, wg_get_field(db, hdr, HDB_SYS_HDR_MEM_BLK_CNT));

    /* Do block list insertion */
    {
	wg_int iter_fld = wg_get_field(db, hdr, HDB_SYS_HDR_MEM_BLK_LIST);

	if (iter_fld == 0) {
	    /* First entry: Prev=NULL, Next=NULL, HDR=this_blk */

	    wg_set_field(db, blk, HDB_MEM_NEXT_BLK,         wg_encode_null(db, 0));
	    wg_set_field(db, blk, HDB_MEM_PREV_BLK,         wg_encode_null(db, 0));

	    wg_set_field(db, hdr, HDB_SYS_HDR_MEM_BLK_LIST, wg_encode_record(db, blk));

	} else if (blk_addr < __get_mem_blk_addr(db, wg_decode_record(db, iter_fld))) {
	    /* Add to the head of the list: 
	     *   Prev=NULL, Next=iter_blk, HDR=this_blk, iter_blk.prev=this_blk 
	     */
	    hdb_mem_t iter_blk = wg_decode_record(db, iter_fld);
	    
	    wg_set_field(db, blk,      HDB_MEM_PREV_BLK,         wg_encode_null   ( db, 0        ));
	    wg_set_field(db, blk,      HDB_MEM_NEXT_BLK,         wg_encode_record ( db, iter_blk ));
	    wg_set_field(db, iter_blk, HDB_MEM_PREV_BLK,         wg_encode_record ( db, blk      ));
	    wg_set_field(db, hdr,      HDB_SYS_HDR_MEM_BLK_LIST, wg_encode_record ( db, blk      ));
	    
	} else {
	    hdb_mem_t iter_blk = wg_decode_record(db, iter_fld);
	    hdb_mem_t next_blk = NULL;
	    wg_int    next_fld = 0;

	    while (1) {
		uintptr_t next_addr = 0;
		
		 next_fld = wg_get_field(db, iter_blk, HDB_MEM_NEXT_BLK);
		
		if (next_fld == 0) {
		    break;
		}

		next_blk  = wg_decode_record(db, next_fld);		
		next_addr = __get_mem_blk_addr(db, next_blk);
		
		if (next_addr > blk_addr) {
		    break;
		}
		
		iter_blk = next_blk;
	    }
	    
	    /* At this point iter_blk must be valid, but next_blk may or may not be NULL 
	     *  Prev = iter_blk, next = next_blk, iter_blk.next = this_blk, if(next_blk) next_blk.prev=this_blk
	     */

	    wg_set_field(db, blk,      HDB_MEM_PREV_BLK,         wg_encode_record(db, iter_blk));
	    wg_set_field(db, iter_blk, HDB_MEM_NEXT_BLK,         wg_encode_record(db, blk));
	    

	    if (next_fld) {
		wg_set_field(db, blk,      HDB_MEM_NEXT_BLK,         wg_encode_record(db, next_blk));
		wg_set_field(db, next_blk, HDB_MEM_PREV_BLK,         wg_encode_record(db, blk));
	    } else {
		wg_set_field(db, blk,      HDB_MEM_NEXT_BLK,         wg_encode_null(db, 0));
	    }	
	}
    }
    
    /* Update header to account for new block */
    wg_set_field(db, hdr, HDB_SYS_HDR_MEM_BLK_CNT, wg_encode_int(db, blk_cnt + 1));


    /* Do free list insertion */
    if (wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_STATE)) == MEMORY_FREE) {
	__insert_free_mem_blk(db, hdr, blk);
    }

    return 0;
}

static uintptr_t 
__allocate_memory(hdb_db_t db,
		  uint32_t numa_node, 
		  uint32_t blk_span)
{
    void    * hdr_rec  = NULL;
    hdb_mem_t iter_blk = NULL;
    uintptr_t ret_addr = NULL;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return 0;
    }
    
    /* Scan free list */
    iter_blk = wg_decode_record(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_FREE_LIST));
    
    if (!iter_blk) {
	ERROR("Could not find any free memory\n");
	return 0;
    }

    /* We assume the system is sane and NUMA interleaving is disabled*/
    while (iter_blk) {
	uint32_t blk_numa = __get_mem_blk_numa(db, iter_blk);

	if ((numa_node == blk_numa) || (numa_node == -1)) {
	    break;
	}

	iter_blk = wg_decode_record(db, wg_get_field(db, iter_blk, HDB_MEM_NEXT_FREE));
    }


    if (blk_span == 1) {

	__remove_free_mem_blk(db, hdr_rec, iter_blk);
	wg_set_field(db, iter_blk, HDB_MEM_STATE, wg_encode_int(db, MEMORY_ALLOCATED));

	return __get_mem_blk_addr(db, iter_blk);	
    } else {
	// allocate a contiguous range
	int i = 0; 

	while (iter_blk) {
	    hdb_mem_t next_free_blk = wg_decode_record(db, wg_get_field(db, iter_blk, HDB_MEM_NEXT_FREE));
	    hdb_mem_t next_blk      = wg_decode_record(db, wg_get_field(db, iter_blk, HDB_MEM_NEXT_BLK));
	    
	    for (i = 0; i < blk_span - 1; i++) {
		
		if (next_blk != next_free_blk) {
		    iter_blk = next_blk;
		    break;
		}
		
		if ((numa_node != -1) &&
		    (numa_node != __get_mem_blk_numa(db, next_blk))) {
		    /* Error because NUMA should not be interleaved */
		    return 0;
		}
		    

		next_free_blk = wg_decode_record(db, wg_get_field(db, next_blk, HDB_MEM_NEXT_FREE));
		next_blk      = wg_decode_record(db, wg_get_field(db, next_blk, HDB_MEM_NEXT_BLK));
	    }
	    
	    if (i == blk_span - 1) {
		break;
	    }
	}

	if (iter_blk == NULL) {
	    return 0;
	}

	ret_addr = __get_mem_blk_addr(db, iter_blk);
	    
	/* Allocate N Blocks starting at iter_blk */
	{
	    
	    for (i = 0; i < blk_span; i++) {
		__remove_free_mem_blk(db, hdr_rec, iter_blk);
		wg_set_field(db, iter_blk, HDB_MEM_STATE, wg_encode_int(db, MEMORY_ALLOCATED));	    
		
		iter_blk = wg_decode_record(db, wg_get_field(db, iter_blk, HDB_MEM_NEXT_FREE));
	    }
	}
    }

    return ret_addr;
}


uintptr_t 
hdb_allocate_memory(hdb_db_t db,
		    uint32_t numa_node, 
		    uint32_t blk_span)
{
    wg_int    lock_id;
    uintptr_t ret = 0;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __allocate_memory(db, numa_node, blk_span);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}


void
hdb_print_mem_free_list(hdb_db_t db)
{

    return;
}


static int
__register_memory(hdb_db_t    db,
		  uintptr_t   base_addr,
		  uint64_t    blk_size,
		  uint32_t    numa_node,
		  mem_state_t state)
{
    void    * hdr_rec  = NULL;
    hdb_mem_t blk      = NULL;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    if (blk_size != wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_SIZE))) {
	ERROR("Memory is not the correct size (requested block size = %lu)\n", blk_size);
	return -1;
    }

    if (__get_mem_blk_by_addr(db, base_addr)) {
	ERROR("Tried to register a memory block (%p) that was already present\n", (void *)base_addr);
	return -1;
    }


    blk = wg_create_record(db, 11);
    wg_set_field(db, blk, HDB_TYPE_FIELD,     wg_encode_int(db, HDB_REC_MEM));
    wg_set_field(db, blk, HDB_MEM_BASE_ADDR,  wg_encode_int(db, base_addr));
    wg_set_field(db, blk, HDB_MEM_BLK_SIZE,   wg_encode_int(db, blk_size));
    wg_set_field(db, blk, HDB_MEM_NUMA_NODE,  wg_encode_int(db, numa_node));
    wg_set_field(db, blk, HDB_MEM_STATE,      wg_encode_int(db, state));
    wg_set_field(db, blk, HDB_MEM_ENCLAVE_ID, wg_encode_int(db, HOBBES_INVALID_ID));
    wg_set_field(db, blk, HDB_MEM_APP_ID,     wg_encode_int(db, HOBBES_INVALID_ID));

    /* Insert block into  */
    if (__insert_mem_blk(db, hdr_rec, blk) == -1) {
	ERROR("Could not insert memory block into blk lists\n");
	return -1;
    }

    return 0;
}


int 
hdb_register_memory(hdb_db_t    db,
		    uintptr_t   base_addr,
		    uint64_t    blk_size,
		    uint32_t    numa_node,
		    mem_state_t state)
{
    wg_int   lock_id;
    int      ret      = -1;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }
    
    ret = __register_memory(db, base_addr, blk_size, numa_node, state);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}


static hobbes_id_t
__get_mem_app_id(hdb_db_t  db,
		     uintptr_t base_addr)
{
    hdb_mem_t   blk    = __get_mem_blk_by_addr(db, base_addr);
    hobbes_id_t app_id = HOBBES_INVALID_ID;

    if (!blk) {
	ERROR("Could not find memory block (%p)\n", (void *)base_addr);
	return -1;
    }
    
    app_id = wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_APP_ID));

    return app_id;
}


hobbes_id_t 
hdb_get_mem_app_id(hdb_db_t  db,
		       uintptr_t base_addr)
{
    hobbes_id_t app_id = HOBBES_INVALID_ID;
    wg_int      lock_id;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    app_id = __get_mem_app_id(db, base_addr);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_ID;
    }

    return app_id;
}


static hobbes_id_t
__get_mem_enclave_id(hdb_db_t  db,
		     uintptr_t base_addr)
{
    hdb_mem_t   blk        = __get_mem_blk_by_addr(db, base_addr);
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    if (!blk) {
	ERROR("Could not find memory block (%p)\n", (void *)base_addr);
	return HOBBES_INVALID_ID;
    }
    
    enclave_id = wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_ENCLAVE_ID));

    return enclave_id;
}


hobbes_id_t 
hdb_get_mem_enclave_id(hdb_db_t  db,
		       uintptr_t base_addr)
{
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;
    wg_int      lock_id;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    enclave_id = __get_mem_enclave_id(db, base_addr);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_ID;
    }

    return enclave_id;
}


static mem_state_t
__get_mem_state(hdb_db_t  db,
		    uintptr_t base_addr)
{
    hdb_mem_t blk     = __get_mem_blk_by_addr(db, base_addr);
    mem_state_t state = MEMORY_INVALID;

    if (!blk) {
	ERROR("Could not find memory block (%p)\n", (void *)base_addr);
	return MEMORY_INVALID;
    }

    state = wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_STATE));

    return state;
}


mem_state_t
hdb_get_mem_state(hdb_db_t  db,
		  uintptr_t base_addr)
{
    wg_int   lock_id;
    mem_state_t state = MEMORY_INVALID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return MEMORY_INVALID;
    }

    state = __get_mem_state(db, base_addr);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return MEMORY_INVALID;
    }

    return state;

}


static uint32_t
__get_mem_numa_node(hdb_db_t  db,
		    uintptr_t base_addr)
{
    hdb_mem_t blk        = __get_mem_blk_by_addr(db, base_addr);
    uint32_t  numa_node  = HOBBES_INVALID_NUMA_ID;

    if (!blk) {
	ERROR("Could not find memory block (%p)\n", (void *)base_addr);
	return HOBBES_INVALID_NUMA_ID;
    }

    numa_node = __get_mem_blk_numa(db, blk);

    return numa_node;
}


uint32_t
hdb_get_mem_numa_node(hdb_db_t  db,
		      uintptr_t base_addr)   
{
    uint32_t numa_node = 0;
    wg_int   lock_id;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_NUMA_ID;
    }

    numa_node = __get_mem_numa_node(db, base_addr);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_NUMA_ID;
    }

    return numa_node;
}




static uintptr_t *
__get_mem_blocks(hdb_db_t   db,
		 uint64_t * num_blks)
{
    uintptr_t * addr_arr = NULL;
    void      * db_rec   = NULL;
    uint64_t    cnt      = 0;
    uint64_t    i        = 0;

    cnt       = __get_sys_blk_cnt(db);
    *num_blks = cnt;

    if ((cnt == 0) || (cnt == -1)) {
	return NULL;
    }

    addr_arr = calloc(sizeof(uint64_t), cnt);

    for (i = 0; i < cnt; i++) {
	db_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_MEM, db_rec);

	if (!db_rec) {
	    ERROR("System Info Header state mismatch\n");
	    cnt = i;
	    break;
	}

	addr_arr[i] = wg_decode_int(db, wg_get_field(db, db_rec, HDB_MEM_BASE_ADDR));
    }

    return addr_arr;
}

uintptr_t * 
hdb_get_mem_blocks(hdb_db_t   db,
		   uint64_t * num_blks)
{
    uintptr_t * addr_arr = NULL;
    wg_int      lock_id;

   if (!num_blks) {
	return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    addr_arr = __get_mem_blocks(db, num_blks);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return addr_arr;
}


/* Debugging */


static void
__sys_print_free_blks(hdb_db_t db)
{
    void * hdr_rec  = __get_sys_hdr(db);

    wg_int iter_fld;

    int i = 0;

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return;
    }

    iter_fld = wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_FREE_LIST);
    
    if (iter_fld == 0) {
	printf("No Free blocks available\n");
	return;
    }

    while (iter_fld) {
	void * iter_blk = wg_decode_record(db, iter_fld);
	char * free_str = NULL;
	
	if (wg_decode_int(db, wg_get_field(db, iter_blk, HDB_MEM_STATE)) == MEMORY_FREE) {
	    free_str = "FREE";
	} else {
	    free_str = "ALLOCATED";
	}

	printf("Free Block %d: [0x%.8lx] (NUMA=%d) <%s>\n", i, 
	       __get_mem_blk_addr(db, iter_blk), 
	       __get_mem_blk_numa(db, iter_blk),
	       free_str);
	
	iter_fld = wg_get_field(db, iter_blk, HDB_MEM_NEXT_FREE);
        i++;
    }
    
    return;
}


void
hdb_sys_print_free_blks(hdb_db_t db) 
{
    wg_int     lock_id;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return;
    }
    
    __sys_print_free_blks(db);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return;
    }

    return;
}
