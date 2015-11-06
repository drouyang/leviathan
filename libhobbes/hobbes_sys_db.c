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

/* 
 * whitedb API wrapper, because their record field API is terrible
 */
static void *
__wg_get_record(hdb_db_t   db, 
		void     * rec, 
		wg_int     field_idx)
{
    wg_int field = wg_get_field(db, rec, field_idx);

    if (field == 0) {
	return NULL;
    } 

    return wg_decode_record(db, field);
}

static void
__wg_set_record(hdb_db_t   db,
		void     * rec,
		wg_int     field_idx,
		void     * value)
{
    if (!value) {
        wg_set_field(db, rec, field_idx, wg_encode_null(db, 0));
    } else {
        wg_set_field(db, rec, field_idx, wg_encode_record(db, value));
    }
}


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

    __wg_set_record(db, rec, HDB_SYS_HDR_MEM_FREE_LIST, NULL);
    __wg_set_record(db, rec, HDB_SYS_HDR_MEM_BLK_LIST,  NULL);
 
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


static hdb_cpu_t
__get_cpu_by_enclave_id(hdb_db_t    db,
			hobbes_id_t enclave_id)
{
    hdb_cpu_t      cpu   = NULL;
    wg_query     * query = NULL;
    wg_query_arg   arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_CPU);

    arglist[1].column = HDB_CPU_ENCLAVE_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, enclave_id);

    query = wg_make_query(db, NULL, 0, arglist, 2);
    
    cpu   = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return cpu;
}

static uint32_t
__get_cpu_id(hdb_db_t  db,
	     hdb_cpu_t cpu)
{
    return wg_decode_int(db, wg_get_field(db, cpu, HDB_CPU_ID));
}

static int
__register_cpu(hdb_db_t    db,
	       uint32_t    cpu_id,
	       uint32_t    numa_node,
	       cpu_state_t state,
	       hobbes_id_t enclave_id)
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
    wg_set_field(db, cpu, HDB_CPU_ENCLAVE_ID,  wg_encode_int(db, enclave_id));

    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, HDB_SYS_HDR_CPU_CNT, wg_encode_int(db, cpu_cnt + 1));

    return 0;
}

int 
hdb_register_cpu(hdb_db_t    db,
		 uint32_t    cpu_id,
		 uint32_t    numa_node,
		 cpu_state_t state,
		 hobbes_id_t enclave_id)
{
    wg_int   lock_id;
    int      ret      = -1;
    
    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __register_cpu(db, cpu_id, numa_node, state, enclave_id);
    
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

static hdb_cpu_t 
__find_free_cpu(hdb_db_t db,
		uint32_t numa_node)
{
    hdb_cpu_t      cpu   = NULL;
    wg_query     * query = NULL;
    wg_query_arg   arglist[3];

    int i      = 0;
    int argcnt = 2;

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_CPU);

    arglist[1].column = HDB_CPU_STATE;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, CPU_FREE);
	
    if (numa_node != HOBBES_ANY_NUMA_ID) {
	arglist[2].column = HDB_CPU_NUMA_NODE;
	arglist[2].cond   = WG_COND_EQUAL;
	arglist[2].value  = wg_encode_query_param_int(db, numa_node);

	argcnt++;
    }

    query = wg_make_query(db, NULL, 0, arglist, argcnt);
    cpu   = wg_fetch(db, query);

    wg_free_query(db, query);

    for (i = 0; i < argcnt; i++) {
	wg_free_query_param(db, arglist[i].value);
    }
    
    return cpu;
}



static uint32_t
__alloc_cpu(hdb_db_t    db,
	    uint32_t    cpu_id,
	    uint32_t    numa_node,
	    hobbes_id_t enclave_id)
{
    hdb_cpu_t cpu    = NULL;
 
    if (cpu_id == HOBBES_ANY_CPU_ID) {
	cpu = __find_free_cpu(db, numa_node);
    } else {
	cpu = __get_cpu_by_id(db, cpu_id);
    }
    
    if (!cpu) {
	ERROR("Could not retrieve CPU for allocation\n");
	return HOBBES_INVALID_CPU_ID;
    }

    cpu_id = __get_cpu_id(db, cpu);

    if (__get_cpu_state(db, cpu_id) != CPU_FREE) {	
	ERROR("CPU (%d) is not available\n", cpu_id);
	return HOBBES_INVALID_CPU_ID;
    }
    
    wg_set_field(db, cpu, HDB_CPU_STATE,      wg_encode_int(db, CPU_ALLOCATED));
    wg_set_field(db, cpu, HDB_CPU_ENCLAVE_ID, wg_encode_int(db, enclave_id)); 

    return cpu_id;
}


uint32_t 
hdb_alloc_cpu(hdb_db_t    db,
	      uint32_t    cpu_id,
	      uint32_t    numa_node,
	      hobbes_id_t enclave_id)
{
    uint32_t ret_val = HOBBES_INVALID_CPU_ID;
    wg_int   lock_id;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_CPU_ID;
    }

    ret_val = __alloc_cpu(db, cpu_id, numa_node, enclave_id);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_CPU_ID;
    }

    return ret_val;
}

static int
__free_cpu(hdb_db_t db,
	   uint32_t cpu_id)
{
    hdb_cpu_t cpu = __get_cpu_by_id(db, cpu_id);

    if (!cpu) {
	ERROR("Could not find CPU (%d)\n", cpu_id);
	return -1;
    }

    if (__get_cpu_state(db, cpu_id) != CPU_ALLOCATED) {
	ERROR("Tried to free unallocated CPU (%d)\n", cpu_id);
	return -1;
    }

    wg_set_field(db, cpu, HDB_CPU_STATE,      wg_encode_int(db, CPU_FREE));
    wg_set_field(db, cpu, HDB_CPU_ENCLAVE_ID, wg_encode_int(db, HOBBES_INVALID_ID));

    return 0;
}


int
hdb_free_cpu(hdb_db_t db,
	     uint32_t cpu_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __free_cpu(db, cpu_id);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return ret;
}

static int
__free_enclave_cpus(hdb_db_t    db,
		    hobbes_id_t enclave_id)
{
    hdb_cpu_t cpu = NULL;

    while ((cpu = __get_cpu_by_enclave_id(db, enclave_id)) != NULL) {
	__free_cpu(db, __get_cpu_id(db, cpu));
    }
    
    return 0;
}

int 
hdb_free_enclave_cpus(hdb_db_t    db,
		      hobbes_id_t enclave_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __free_enclave_cpus(db, enclave_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return -1;
    }

    return ret;
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


static hdb_mem_t
__get_mem_blk_by_enclave_id(hdb_db_t    db,
			    hobbes_id_t enclave_id)
{
    hdb_mem_t      blk   = NULL;
    wg_query     * query = NULL;
    wg_query_arg   arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_MEM);

    arglist[1].column = HDB_MEM_ENCLAVE_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, enclave_id);

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
    hdb_mem_t next_blk = __wg_get_record(db, blk, HDB_MEM_NEXT_FREE);
    hdb_mem_t prev_blk = __wg_get_record(db, blk, HDB_MEM_PREV_FREE);

    if (wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_STATE)) != MEMORY_FREE) {
	ERROR("Tried to remove a non-free memory block from free list\n");
	return -1;
    }

    /* Update the head of the list if necessary */
    

    if (__wg_get_record(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST) == blk) {
	__wg_set_record(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST, next_blk);
    }

    if (next_blk) {
	__wg_set_record(db, next_blk, HDB_MEM_PREV_FREE, prev_blk);
    }

    if (prev_blk) {
	__wg_set_record(db, prev_blk, HDB_MEM_NEXT_FREE, next_blk);
    }

    __wg_set_record(db, blk, HDB_MEM_NEXT_FREE, NULL);
    __wg_set_record(db, blk, HDB_MEM_PREV_FREE, NULL);
    
   {
	uint64_t free_mem = wg_decode_int(db, wg_get_field(db, hdr, HDB_SYS_HDR_MEM_FREE_BLK_CNT));
	wg_set_field(db, hdr, HDB_SYS_HDR_MEM_FREE_BLK_CNT, wg_encode_int(db, free_mem - 1));
	
    }

    return 0;
}



static int 
__insert_free_mem_blk(hdb_db_t   db, 
		      void     * hdr,
		      hdb_mem_t  blk)
{
    uintptr_t blk_addr = __get_mem_blk_addr(db, blk);
    hdb_mem_t iter_blk = __wg_get_record(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST);

    if (iter_blk == NULL) {
	/* First entry: Prev=NULL, Next=NULL, HDR=this_blk */
	    
	__wg_set_record(db, blk, HDB_MEM_NEXT_FREE,         NULL);
	__wg_set_record(db, blk, HDB_MEM_PREV_FREE,         NULL);
	__wg_set_record(db, hdr, HDB_SYS_HDR_MEM_FREE_LIST, blk);
	    
    } else if (blk_addr < __get_mem_blk_addr(db, iter_blk)) {
	/* Add to the head of the list: 
	 *   Prev=NULL, Next=iter_blk, HDR=this_blk, iter_blk.prev=this_blk 
	 */

	__wg_set_record(db, blk,      HDB_MEM_PREV_FREE,         NULL);
	__wg_set_record(db, blk,      HDB_MEM_NEXT_FREE,         iter_blk);
	__wg_set_record(db, iter_blk, HDB_MEM_PREV_FREE,         blk);
	__wg_set_record(db, hdr,      HDB_SYS_HDR_MEM_FREE_LIST, blk);

    } else {
	hdb_mem_t next_blk = NULL;

	while (1) {
	    uintptr_t next_addr = 0;

	    next_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_FREE);
		
	    if (next_blk == NULL) {
		break;
	    }

	    next_addr = __get_mem_blk_addr(db, next_blk);
		
	    if (next_addr > blk_addr) {
		break;
	    }
		
	    iter_blk = next_blk;
	}
	
	    
	/* At this point iter_blk must be valid, but next_blk may or may not be NULL 
	 *  Prev = iter_blk, next = next_blk, iter_blk.next = this_blk, if(next_blk) next_blk.prev=this_blk
	 */

	__wg_set_record(db, blk,      HDB_MEM_PREV_FREE, iter_blk);
	__wg_set_record(db, blk,      HDB_MEM_NEXT_FREE, next_blk);
	__wg_set_record(db, iter_blk, HDB_MEM_NEXT_FREE, blk);

	if (next_blk) {
	    __wg_set_record(db, next_blk, HDB_MEM_PREV_FREE, blk);
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
	hdb_mem_t iter_blk = __wg_get_record(db, hdr, HDB_SYS_HDR_MEM_BLK_LIST);

	if (iter_blk == NULL) {
	    /* First entry: Prev=NULL, Next=NULL, HDR=this_blk */

	    __wg_set_record(db, blk, HDB_MEM_NEXT_BLK,         NULL);
	    __wg_set_record(db, blk, HDB_MEM_PREV_BLK,         NULL);

	    __wg_set_record(db, hdr, HDB_SYS_HDR_MEM_BLK_LIST, blk);

	} else if (blk_addr < __get_mem_blk_addr(db, iter_blk)) {
	    /* Add to the head of the list: 
	     *   Prev=NULL, Next=iter_blk, HDR=this_blk, iter_blk.prev=this_blk 
	     */
	    
	    __wg_set_record(db, blk,      HDB_MEM_PREV_BLK,         NULL     );
	    __wg_set_record(db, blk,      HDB_MEM_NEXT_BLK,         iter_blk );
	    __wg_set_record(db, iter_blk, HDB_MEM_PREV_BLK,         blk      );
	    __wg_set_record(db, hdr,      HDB_SYS_HDR_MEM_BLK_LIST, blk      );
	    
	} else {
	    hdb_mem_t next_blk = NULL;

	    while (1) {
		uintptr_t next_addr = 0;
		
		next_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_BLK);
		
		if (next_blk == NULL) {
		    break;
		}

		next_addr = __get_mem_blk_addr(db, next_blk);
		
		if (next_addr > blk_addr) {
		    break;
		}
		
		iter_blk = next_blk;
	    }
	    
	    /* At this point iter_blk must be valid, but next_blk may or may not be NULL 
	     *  Prev = iter_blk, next = next_blk, iter_blk.next = this_blk, if(next_blk) next_blk.prev=this_blk
	     */

	    __wg_set_record(db, blk,      HDB_MEM_PREV_BLK, iter_blk);
	    __wg_set_record(db, iter_blk, HDB_MEM_NEXT_BLK, blk);
	    __wg_set_record(db, blk,      HDB_MEM_NEXT_BLK, next_blk);

	    if (next_blk) {
		__wg_set_record(db, next_blk, HDB_MEM_PREV_BLK, blk);
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


static int
__free_block(hdb_db_t  db,
	     hdb_mem_t blk)
{
    void * hdr = NULL;

    hdr = __get_sys_hdr(db);

    if (!hdr) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    wg_set_field(db, blk, HDB_MEM_ENCLAVE_ID, wg_encode_int(db, HOBBES_INVALID_ID));
    wg_set_field(db, blk, HDB_MEM_APP_ID,     wg_encode_int(db, HOBBES_INVALID_ID));
    wg_set_field(db, blk, HDB_MEM_STATE,      wg_encode_int(db, MEMORY_FREE));

    __insert_free_mem_blk(db, hdr, blk);

    return 0;
}


static int
__free_span(hdb_db_t  db,
	    uintptr_t base_addr,
	    uint32_t  block_span)
{
    hdb_mem_t blk = __get_mem_blk_by_addr(db, base_addr);
    uint32_t  i   = 0;

    if (blk == NULL) {
	ERROR("Tried to free and invalid block at (%p)\n", (void *)base_addr);
	return -1;
    }

    for (i = 0; i < block_span; i++) {

	__free_block(db, blk);

	blk = __wg_get_record(db, blk, HDB_MEM_NEXT_BLK);
    }
    
    return 0;
}


int 
hdb_free_block(hdb_db_t  db,
	       uintptr_t base_addr,
	       uint32_t  block_span)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __free_span(db, base_addr, block_span);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}

static int
__free_enclave_blocks(hdb_db_t    db,
		      hobbes_id_t enclave_id)
{
    hdb_mem_t blk = NULL;

    while ((blk = __get_mem_blk_by_enclave_id(db, enclave_id)) != NULL) {
	__free_block(db, blk);
    }

    return 0;
}


int
hdb_free_enclave_blocks(hdb_db_t    db,
			hobbes_id_t enclave_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __free_enclave_blocks(db, enclave_id);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}

static int
__alloc_block(hdb_db_t    db,
	      hdb_mem_t   blk,
	      hobbes_id_t enclave_id)
{
    void    * hdr_rec  = NULL;    

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    if (blk == NULL) {
	ERROR("Tried to allocate and invalid block\n");
	return -1;
    }

    __remove_free_mem_blk(db, hdr_rec, blk);
    wg_set_field(db, blk, HDB_MEM_STATE,      wg_encode_int(db, MEMORY_ALLOCATED));
    wg_set_field(db, blk, HDB_MEM_ENCLAVE_ID, wg_encode_int(db, enclave_id));

    return 0;
}


static uintptr_t 
__alloc_span(hdb_db_t    db,
	     hobbes_id_t enclave_id,
	     uint32_t    numa_node, 
	     uint32_t    blk_span)
{
    void    * hdr_rec  = NULL;
    hdb_mem_t iter_blk = NULL;
    uintptr_t ret_addr = 0;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }
    
    /* Scan free list */
    iter_blk = __wg_get_record(db, hdr_rec, HDB_SYS_HDR_MEM_FREE_LIST);
    
    if (!iter_blk) {
	ERROR("Could not find any free memory\n");
	return -1;
    }

    /* We assume the system is sane and NUMA interleaving is disabled*/
    while (iter_blk) {
	uint32_t blk_numa = __get_mem_blk_numa(db, iter_blk);

	if ((numa_node == blk_numa) || (numa_node == HOBBES_ANY_NUMA_ID)) {
	    break;
	}

	iter_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_FREE);
    }


    if (iter_blk == NULL) {
	/* Couldn't find anything */
	return -1;
    }

    if (blk_span == 1) {
	__alloc_block(db, iter_blk, enclave_id);

	return __get_mem_blk_addr(db, iter_blk);	
    } else {
	// allocate a contiguous range
	uint32_t i = 0; 

	while (iter_blk) {
	    hdb_mem_t next_free_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_FREE);
	    hdb_mem_t next_blk      = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_BLK);
	    
	    for (i = 0; i < blk_span - 1; i++) {
		
		if (next_blk != next_free_blk) {
		    iter_blk = next_blk;
		    break;
		}
		
		if ((numa_node != HOBBES_ANY_NUMA_ID) &&
		    (numa_node != __get_mem_blk_numa(db, next_blk))) {
		    /* Error because NUMA should not be interleaved */
		    return -1;
		}
		    

		next_free_blk = __wg_get_record(db, next_blk, HDB_MEM_NEXT_FREE);
		next_blk      = __wg_get_record(db, next_blk, HDB_MEM_NEXT_BLK);
	    }
	    
	    if (i == blk_span - 1) {
		break;
	    }
	}

	if (iter_blk == NULL) {
	    return -1;
	}

	ret_addr = __get_mem_blk_addr(db, iter_blk);
	    
	printf("allocating span starting at %p\n", (void *)ret_addr);
	/* Allocate N Blocks starting at iter_blk */
	{
	    
	    for (i = 0; i < blk_span; i++) {
		
		printf("Allocating block %d of span\n", i);
		__alloc_block(db, iter_blk, enclave_id);
		
		iter_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_BLK);
	    }
	}
    }

    return ret_addr;
}


uintptr_t 
hdb_alloc_block(hdb_db_t    db,
		hobbes_id_t enclave_id,
		uint32_t    numa_node, 
		uint32_t    blk_span)
{
    wg_int    lock_id;
    uintptr_t ret = 0;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __alloc_span(db, enclave_id, numa_node, blk_span);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}


static int
__alloc_blocks(hdb_db_t    db,
	       hobbes_id_t enclave_id,
	       uint32_t    numa_node, 
	       uint32_t    num_blocks,
	       uint32_t    block_span,
	       uintptr_t * block_array)
{
    uint32_t i   = 0;
    int      ret = 0;

    memset(block_array, 0, sizeof(uintptr_t) * num_blocks);

    for (i = 0; i < num_blocks; i++) {
	block_array[i] = __alloc_span(db, enclave_id, numa_node, block_span);

	if (block_array[i] == (uintptr_t)-1) {
	    ret = -1;
	    break;
	}
    }
    
    if (ret == -1) {

	for (i = 0; i < num_blocks; i++) {
	    
	    if ((block_array[i] != 0) && (block_array[i] != (uintptr_t)-1)) {
		__free_span(db, block_array[i], block_span);
	    }
	}
    }
    
    return ret;
}


int
hdb_alloc_blocks(hdb_db_t    db,
		 hobbes_id_t enclave_id,
		 uint32_t    numa_node,
		 uint32_t    num_blocks, 
		 uint32_t    block_span, 
		 uintptr_t * block_array)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __alloc_blocks(db, enclave_id, numa_node, num_blocks, block_span, block_array);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    
    return ret;
}




static int
__alloc_block_addr(hdb_db_t    db,
		   hobbes_id_t enclave_id,
		   uintptr_t   base_addr)
{
    void      * hdr_rec = NULL;
    hdb_mem_t   blk     = NULL;
    mem_state_t state   = MEMORY_INVALID;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    blk = __get_mem_blk_by_addr(db, base_addr);

    if (!blk) {
	ERROR("Could not get memory block for address (%p)\n", (void *)base_addr);
	return -1;
    }

    state = wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_STATE));

    if (state != MEMORY_FREE) {
	ERROR("Memory block (addr=%p) is not available\n", (void *)base_addr);
	return -1;
    }

    __alloc_block(db, blk, enclave_id);

    return 0;
}





static int
__alloc_blocks_addr(hdb_db_t    db,
		    hobbes_id_t enclave_id,
		    uint32_t    block_span,
		    uintptr_t   base_addr)
{
    uintptr_t tmp_addr = base_addr;
    uint32_t  i        = 0;
    
    
    for (i = 0; i < block_span; i++) {

	if (__alloc_block_addr(db, enclave_id, tmp_addr) != 0) {
	    __free_span(db, base_addr, i);
	    return -1;
	}
	
	tmp_addr += __get_sys_blk_size(db);
    }
 
    return 0;
}

int
hdb_alloc_block_addr(hdb_db_t    db,
		     hobbes_id_t enclave_id, 
		     uint32_t    block_span,
		     uintptr_t   base_addr)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __alloc_blocks_addr(db, enclave_id, block_span, base_addr);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret;
}




static int
__register_memory(hdb_db_t    db,
		  uintptr_t   base_addr,
		  uint64_t    blk_size,
		  uint32_t    numa_node,
		  mem_state_t state,
		  hobbes_id_t enclave_id)
{
    void    * hdr_rec  = NULL;
    hdb_mem_t blk      = NULL;

    hdr_rec = __get_sys_hdr(db);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    if (blk_size != (uint64_t)wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_SIZE))) {
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
    wg_set_field(db, blk, HDB_MEM_ENCLAVE_ID, wg_encode_int(db, enclave_id));
    wg_set_field(db, blk, HDB_MEM_APP_ID,     wg_encode_int(db, HOBBES_INVALID_ID));

    __wg_set_record(db, blk, HDB_MEM_NEXT_FREE, NULL);
    __wg_set_record(db, blk, HDB_MEM_PREV_FREE, NULL);
    __wg_set_record(db, blk, HDB_MEM_NEXT_BLK,  NULL);
    __wg_set_record(db, blk, HDB_MEM_PREV_BLK,  NULL);

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
		    mem_state_t state,
		    hobbes_id_t enclave_id)
{
    wg_int   lock_id;
    int      ret      = -1;

    lock_id = wg_start_write(db);
    
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }
    
    ret = __register_memory(db, base_addr, blk_size, numa_node, state, enclave_id);

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

    if ((cnt == 0) || (cnt == (uint64_t)-1)) {
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
    void    * hdr_rec  = __get_sys_hdr(db);
    hdb_mem_t iter_blk = NULL;

    int i = 0;

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return;
    }

    iter_blk = __wg_get_record(db, hdr_rec, HDB_SYS_HDR_MEM_FREE_LIST);
    
    if (iter_blk == NULL) {
	printf("No Free blocks available\n");
	return;
    }

    while (iter_blk) {
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
	
	iter_blk = __wg_get_record(db, iter_blk, HDB_MEM_NEXT_FREE);
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
