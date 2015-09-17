/* Hobbes Database interface for whitedb
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
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


/* 
 * Database Record Type
 *    Each row in the database has an associated type from the list below
 */
#define HDB_REC_ENCLAVE          0
#define HDB_REC_APP              1
#define HDB_REC_SEGMENT          2
#define HDB_REC_ENCLAVE_HDR      3
#define HDB_REC_APP_HDR          4
#define HDB_REC_XEMEM_HDR        5
#define HDB_REC_XEMEM_SEGMENT    6
#define HDB_REC_XEMEM_ATTACHMENT 7
#define HDB_REC_PMI_KEYVAL       8
#define HDB_REC_PMI_BARRIER      9
#define HDB_REC_SYS_HDR          10
#define HDB_REC_CPU              11
#define HDB_REC_MEM              12


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
#define HDB_SEGMENT_ENCLAVE  3
#define HDB_SEGMENT_APP      4

/* Columns for application header */
#define HDB_APP_HDR_NEXT 1
#define HDB_APP_HDR_CNT  2

/* Columns for application records */
#define HDB_APP_ID       1
#define HDB_APP_NAME     2
#define HDB_APP_STATE    3
#define HDB_APP_ENCLAVE  4

/* Columns for PMI key value store records */
#define HDB_PMI_KVS_ENTRY_APPID   1
#define HDB_PMI_KVS_ENTRY_KVSNAME 2
#define HDB_PMI_KVS_ENTRY_KEY     3
#define HDB_PMI_KVS_ENTRY_VALUE   4


/* Columns for PMI barrier records */
#define HDB_PMI_BARRIER_APPID   1
#define HDB_PMI_BARRIER_COUNTER 2
#define HDB_PMI_BARRIER_SEGIDS	3

/* Columns for System Information */
#define HDB_SYS_HDR_CPU_CNT           1
#define HDB_SYS_HDR_NUMA_CNT          2
#define HDB_SYS_HDR_MEM_BLK_SIZE      3
#define HDB_SYS_HDR_MEM_BLK_CNT       4
#define HDB_SYS_HDR_MEM_FREE_BLK_CNT  5


/* Columns for memory resource records */
#define HDB_MEM_BASE_ADDR  1
#define HDB_MEM_BLK_SIZE   2
#define HDB_MEM_NUMA_NODE  3
#define HDB_MEM_STATE      4
#define HDB_MEM_ENCLAVE_ID 5
#define HDB_MEM_APP_ID     6

/* Columns for CPU resource records */
#define HDB_CPU_ID         1
#define HDB_CPU_NUMA_NODE  2
#define HDB_CPU_STATE      3
#define HDB_CPU_ENCLAVE_ID 4


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
    
    /* Create Enclave Header */
    rec = wg_create_record(db, 3);
    wg_set_field(db, rec, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_ENCLAVE_HDR));
    wg_set_field(db, rec, HDB_ENCLAVE_HDR_NEXT, wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_ENCLAVE_HDR_CNT,  wg_encode_int(db, 0));
    
    /* Create Application Header */
    rec = wg_create_record(db, 3);
    wg_set_field(db, rec, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_APP_HDR));
    wg_set_field(db, rec, HDB_APP_HDR_NEXT,     wg_encode_int(db, 1));
    wg_set_field(db, rec, HDB_APP_HDR_CNT,      wg_encode_int(db, 0));
    
    /* Create XEMEM header */
    rec = wg_create_record(db, 2);
    wg_set_field(db, rec, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_XEMEM_HDR));
    wg_set_field(db, rec, HDB_SEGMENT_HDR_CNT,  wg_encode_int(db, 0));

    /* Create System Info Header */
    rec = wg_create_record(db, 6);
    wg_set_field(db, rec, HDB_TYPE_FIELD,               wg_encode_int(db, HDB_REC_SYS_HDR));
    wg_set_field(db, rec, HDB_SYS_HDR_CPU_CNT,          wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_NUMA_CNT,         wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_BLK_SIZE,     wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_BLK_CNT,      wg_encode_int(db, 0));
    wg_set_field(db, rec, HDB_SYS_HDR_MEM_FREE_BLK_CNT, wg_encode_int(db, 0));


    return 0;
}


void * 
hdb_get_db_addr(hdb_db_t db) 
{
#ifdef USE_DATABASE_HANDLE
   return ((db_handle *)db)->db;
#else
    return db;
#endif
}


/*
 * System Info Database records
 */

static int
__init_system_info(hdb_db_t db,
		   uint32_t numa_nodes,
		   uint64_t mem_blk_size)
{
    void * hdr_rec = NULL;
    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }
    
    wg_set_field(db, hdr_rec, HDB_SYS_HDR_NUMA_CNT,         wg_encode_int(db, numa_nodes));
    wg_set_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_SIZE,     wg_encode_int(db, mem_blk_size));

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
    void     * hdr_rec  = NULL;
    uint32_t   numa_cnt = 0;

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

    if (!hdr_rec) {
	ERROR("Malformed database. Missing System Info Header\n");
	return (uint32_t)-1;
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

    printf("db: %p\n", db);

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

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

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

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

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

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
    hdb_cpu_t       cpu   = NULL;
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
    void     * hdr_rec  = NULL;
    uint32_t   cpu_cnt  = 0;
    hdb_cpu_t  cpu      = NULL;

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

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

    printf("db:%p\n", db);

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

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


static int
__register_memory(hdb_db_t    db,
		  uintptr_t   base_addr,
		  uint64_t    blk_size,
		  uint32_t    numa_node,
		  mem_state_t state)
{
    void    * hdr_rec  = NULL;
    uint64_t  blk_cnt  = 0;
    hdb_mem_t blk      = NULL;

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_SYS_HDR, NULL);

    if (!hdr_rec) {
	ERROR("Malformed Database. Missing System Info Header\n");
	return -1;
    }

    if (__get_mem_blk_by_addr(db, base_addr)) {
	ERROR("Tried to register a memory block (%p) that was already present\n", (void *)base_addr);
	return -1;
    }

    blk_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_CNT));

    blk = wg_create_record(db, 7);
    wg_set_field(db, blk, HDB_TYPE_FIELD,     wg_encode_int(db, HDB_REC_MEM));
    wg_set_field(db, blk, HDB_MEM_BASE_ADDR,  wg_encode_int(db, base_addr));
    wg_set_field(db, blk, HDB_MEM_BLK_SIZE,   wg_encode_int(db, blk_size));
    wg_set_field(db, blk, HDB_MEM_NUMA_NODE,  wg_encode_int(db, numa_node));
    wg_set_field(db, blk, HDB_MEM_STATE,      wg_encode_int(db, state));
    wg_set_field(db, blk, HDB_MEM_ENCLAVE_ID, wg_encode_int(db, HOBBES_INVALID_ID));
    wg_set_field(db, blk, HDB_MEM_APP_ID,     wg_encode_int(db, HOBBES_INVALID_ID));

    /* TODO: Add a flow link to the next physical contiguous block for contiguous allocations */
    /* TODO: Add flow link from previous physically contiguous block for contiguous allocations */

    /* Update the enclave Header information */
    wg_set_field(db, hdr_rec, HDB_SYS_HDR_MEM_BLK_CNT, wg_encode_int(db, blk_cnt + 1));

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

    numa_node = wg_decode_int(db, wg_get_field(db, blk, HDB_MEM_NUMA_NODE));

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


/* 
 * Enclave Accessors 
 */


/**
 * Get an enclave handle from an enclave id
 *  - Returns NULL if no enclave is found 
 **/
static hdb_enclave_t
__get_enclave_by_id(hdb_db_t    db, 
		    hobbes_id_t enclave_id) 
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

    query   = wg_make_query(db, NULL, 0, arglist, 2);

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





static hobbes_id_t
__create_enclave_record(hdb_db_t         db,
			char           * name, 
			int              mgmt_dev_id, 
			enclave_type_t   type, 
			hobbes_id_t      parent)
{
    void       * hdr_rec       = NULL;
    hobbes_id_t  enclave_id    = HOBBES_INVALID_ID;
    uint32_t     enclave_cnt   = 0;
    char         auto_name[32] = {[0 ... 31] = 0};

    hdb_enclave_t enclave   = NULL;

    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing enclave Header\n");
	return HOBBES_INVALID_ID;
    }
    
    if (parent == HOBBES_INVALID_ID) {
	/* The Master enclave doesn't have a parent  *
	 * and gets a well known ID                  */
	enclave_id = HOBBES_MASTER_ENCLAVE_ID;
    } else {
	/* Get Next Available enclave ID */
	enclave_id  = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_NEXT));
    }

    /* Verify that enclave_id is available */
    if (__get_enclave_by_id(db, enclave_id)) {
	return HOBBES_INVALID_ID;
    }

    enclave_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT));
 
    if (name == NULL) {
	snprintf(auto_name, 31, "enclave-%d", enclave_id);
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

hobbes_id_t
hdb_create_enclave(hdb_db_t         db,
		   char           * name, 
		   int              mgmt_dev_id, 
		   enclave_type_t   type, 
		   hobbes_id_t      parent)
{
    wg_int      lock_id;
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    enclave_id = __create_enclave_record(db, name, mgmt_dev_id, type, parent);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }
    

    return enclave_id;
}

static int
__delete_enclave(hdb_db_t    db,
		 hobbes_id_t enclave_id)
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
hdb_delete_enclave(hdb_db_t    db,
		   hobbes_id_t enclave_id)
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
__get_enclave_dev_id(hdb_db_t    db, 
		     hobbes_id_t enclave_id)
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
hdb_get_enclave_dev_id(hdb_db_t    db, 
		       hobbes_id_t enclave_id)
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

static hobbes_id_t
__get_enclave_parent(hdb_db_t    db,
		     hobbes_id_t enclave_id)
{
    hdb_enclave_t enclave   = NULL;
    hobbes_id_t   parent_id = HOBBES_INVALID_ID;

    enclave = __get_enclave_by_id(db, enclave_id);

    if (enclave == NULL) {
	ERROR("could not find enclave (id: %d)\n", enclave_id);
	return HOBBES_INVALID_ID;
    }

    parent_id = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_PARENT));

    return parent_id;
}


hobbes_id_t
hdb_get_enclave_parent(hdb_db_t    db,
		       hobbes_id_t enclave_id)
{
    wg_int      lock_id;
    hobbes_id_t parent_id = HOBBES_INVALID_ID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    parent_id = __get_enclave_parent(db, enclave_id);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }

    return parent_id;
}


static int 
__set_enclave_dev_id(hdb_db_t    db, 
		     hobbes_id_t enclave_id, 
		     int         dev_id)
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
hdb_set_enclave_dev_id(hdb_db_t    db, 
		       hobbes_id_t enclave_id, 
		       int         dev_id)
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
__get_enclave_type(hdb_db_t    db, 
		   hobbes_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    enclave_type_t type = INVALID_ENCLAVE;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return INVALID_ENCLAVE;
    }

    type = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_TYPE));

    return type;
}

enclave_type_t
hdb_get_enclave_type(hdb_db_t    db, 
		     hobbes_id_t enclave_id)
{
    wg_int lock_id;
    enclave_type_t type = INVALID_ENCLAVE;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return INVALID_ENCLAVE;
    }

    type = __get_enclave_type(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return INVALID_ENCLAVE;
    }
    

    return type;
}




static enclave_state_t
__get_enclave_state(hdb_db_t    db, 
		    hobbes_id_t enclave_id)
{
    hdb_enclave_t enclave = NULL;

    enclave_state_t state = ENCLAVE_ERROR;

    enclave = __get_enclave_by_id(db, enclave_id);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (id: %d)\n", enclave_id);
	return ENCLAVE_ERROR;
    }

    state = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_STATE));

    return state;
}

enclave_state_t
hdb_get_enclave_state(hdb_db_t    db, 
		      hobbes_id_t enclave_id)
{
    wg_int lock_id;
    enclave_state_t state = ENCLAVE_ERROR;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return ENCLAVE_ERROR;
    }

    state = __get_enclave_state(db, enclave_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return ENCLAVE_ERROR;
    }
    

    return state;
}


static int
__set_enclave_state(hdb_db_t        db, 
		    hobbes_id_t     enclave_id, 
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
		      hobbes_id_t     enclave_id, 
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
__get_enclave_name(hdb_db_t    db, 
		   hobbes_id_t enclave_id)
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
hdb_get_enclave_name(hdb_db_t    db, 
		     hobbes_id_t enclave_id)
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
__get_enclave_cmdq(hdb_db_t    db, 
		   hobbes_id_t enclave_id)
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
hdb_get_enclave_cmdq(hdb_db_t    db,
		     hobbes_id_t enclave_id)
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
		   hobbes_id_t   enclave_id, 
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
		     hobbes_id_t   enclave_id, 
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



static hobbes_id_t
__get_enclave_id(hdb_db_t   db, 
		 char     * enclave_name)
{
    hdb_enclave_t enclave    = NULL;
    hobbes_id_t   enclave_id = HOBBES_INVALID_ID;

    enclave = __get_enclave_by_name(db, enclave_name);
    
    if (enclave == NULL) {
	ERROR("Could not find enclave (name: %s)\n", enclave_name);
	return HOBBES_INVALID_ID;
    }

    enclave_id = wg_decode_int(db, wg_get_field(db, enclave, HDB_ENCLAVE_ID));

    return enclave_id;
}

hobbes_id_t
hdb_get_enclave_id(hdb_db_t   db, 
		   char     * enclave_name)
{
    wg_int      lock_id;
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

   enclave_id = __get_enclave_id(db, enclave_name);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }
    
    return enclave_id;
}

static hobbes_id_t *
__get_enclaves(hdb_db_t   db,
	      int       * num_enclaves)
{
    hobbes_id_t * id_arr  = NULL;
    void        * db_rec  = NULL;
    void        * hdr_rec = NULL;
    int           cnt     = 0;
    int           i       = 0;
    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_ENCLAVE_HDR, NULL);    

    if (!hdr_rec) {
	ERROR("Malformed database. Missing enclave Header\n");
	return NULL;
    }

    cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_ENCLAVE_HDR_CNT));

    id_arr = calloc(sizeof(hobbes_id_t), cnt);

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


hobbes_id_t * 
hdb_get_enclaves(hdb_db_t   db,
		 int      * num_enclaves)
{
    hobbes_id_t * id_arr = NULL;
    wg_int        lock_id;

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






/* *******
 * 
 *  XEMEM 
 * 
 * *******/

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
			char          * name,
			hobbes_id_t     enclave_id,
			hobbes_id_t     app_id)
{
    void * hdr_rec        = NULL;
    void * rec            = NULL;
    int    segment_cnt    = 0;

    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_XEMEM_HDR, NULL);
    
    if (!hdr_rec) {
        ERROR("malformed database. Missing xemem Header\n");
        return -1;
    }

    /* Ensure segid and name do not exist */
    rec = __get_segment_by_segid(db, segid);
    if (rec) {
        ERROR("xemem segment with segid %ld already present\n", segid);
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
    rec = wg_create_record(db, 5);
    wg_set_field(db, rec, HDB_TYPE_FIELD,       wg_encode_int(db, HDB_REC_XEMEM_SEGMENT));
    wg_set_field(db, rec, HDB_SEGMENT_SEGID,    wg_encode_int(db, segid));
    wg_set_field(db, rec, HDB_SEGMENT_NAME,     wg_encode_str(db, name, NULL));
    wg_set_field(db, rec, HDB_SEGMENT_ENCLAVE,  wg_encode_int(db, enclave_id));
    wg_set_field(db, rec, HDB_SEGMENT_APP,      wg_encode_int(db, app_id));

    /* Update the xemem Header information */
    segment_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT));
    wg_set_field(db, hdr_rec, HDB_SEGMENT_HDR_CNT, wg_encode_int(db, segment_cnt + 1));

    return 0;
}


int
hdb_create_xemem_segment(hdb_db_t      db,
			 xemem_segid_t segid,
			 char        * name, 
			 hobbes_id_t   enclave_id,
			 hobbes_id_t   app_id)
{
    wg_int lock_id;
    int    ret;

    lock_id = wg_start_write(db);
    if (!lock_id) {
        ERROR("Could not lock database\n");
        return -1;
    }

    ret = __create_segment_record(db, segid, name, enclave_id, app_id);

    if (!wg_end_write(db, lock_id)) {
        ERROR("Apparently this is catastrophic...\n");
	return -1;
    }


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
	ERROR("Could not find xemem segment (segid: %ld)\n", segid);
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

    if (!wg_end_write(db, lock_id)) {
        ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret;

}

static xemem_segid_t
__get_xemem_segid(hdb_db_t   db,
		  char     * name)
{
    hdb_segment_t segment = NULL;
    xemem_segid_t segid   = XEMEM_INVALID_SEGID;

    segment = __get_segment_by_name(db, name);

    if (segment == NULL) {
	ERROR("Could not find XEMEM segment (name: %s)\n", name);
	return XEMEM_INVALID_SEGID;
    }

    segid = wg_decode_int(db, wg_get_field(db, segment, HDB_SEGMENT_SEGID));
    
    return segid;
}


xemem_segid_t
hdb_get_xemem_segid(hdb_db_t   db,
		    char     * name)
{
    wg_int lock_id;
    xemem_segid_t segid = XEMEM_INVALID_SEGID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return XEMEM_INVALID_SEGID;
    }

    segid = __get_xemem_segid(db, name);

    if (!wg_end_read(db, lock_id)) {
        ERROR("Catastrophic database locking error\n");
	return XEMEM_INVALID_SEGID;
    }

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
	ERROR("Could not find XEMEM segment (id: %ld)\n", segid);
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

    if (!wg_end_read(db, lock_id)) {
        ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return name;
}



hobbes_id_t
__get_xemem_enclave(hdb_db_t        db,
		    xemem_segid_t   segid)
{
    hdb_segment_t segment    = NULL;
    hobbes_id_t   enclave_id = HOBBES_INVALID_ID;

    segment = __get_segment_by_segid(db, segid);

    if (segment == NULL) {
	ERROR("Could not find XEMEM segment (id: %ld)\n", segid);
	return HOBBES_INVALID_ID;
    }

    enclave_id = wg_decode_int(db, wg_get_field(db, segment, HDB_SEGMENT_ENCLAVE));

    return enclave_id;
}


hobbes_id_t
hdb_get_xemem_enclave(hdb_db_t      db,
		      xemem_segid_t segid)
{
    wg_int lock_id;
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return HOBBES_INVALID_ID;
    }

    enclave_id = __get_xemem_enclave(db, segid);

    if (!wg_end_read(db, lock_id)) {
        ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_ID;
    }

    return enclave_id;
}

hobbes_id_t
__get_xemem_app(hdb_db_t        db,
		xemem_segid_t   segid)
{
    hdb_segment_t segment = NULL;
    hobbes_id_t   app_id  = HOBBES_INVALID_ID;

    segment = __get_segment_by_segid(db, segid);

    if (segment == NULL) {
	ERROR("Could not find XEMEM segment (id: %ld)\n", segid);
	return HOBBES_INVALID_ID;
    }

    app_id = wg_decode_int(db, wg_get_field(db, segment, HDB_SEGMENT_APP));

    return app_id;
}


hobbes_id_t
hdb_get_xemem_app(hdb_db_t      db,
		  xemem_segid_t segid)
{
    wg_int lock_id;
    hobbes_id_t app_id = HOBBES_INVALID_ID;
    
    lock_id = wg_start_read(db);

    if (!lock_id) {
        ERROR("Could not lock database\n");
        return HOBBES_INVALID_ID;
    }

    app_id = __get_xemem_app(db, segid);

    if (!wg_end_read(db, lock_id)) {
        ERROR("Catastrophic database locking error\n");
	return HOBBES_INVALID_ID;
    }

    return app_id;
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




/* *******
 * 
 *  Applications
 * 
 * *******/


/**
 * Get an app handle from an app id
 *  - Returns NULL if no app is found 
 **/
static hdb_app_t
__get_app_by_id(hdb_db_t    db, 
		hobbes_id_t app_id) 
{
    hdb_app_t     app  = NULL;
    wg_query    * query    = NULL;
    wg_query_arg  arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_APP);    

    arglist[1].column = HDB_APP_ID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, app_id);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    app = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return app;
}





/**
 * Get an app handle from an app name
 *  - Returns NULL if no app is found 
 **/
static hdb_app_t
__get_app_by_name(hdb_db_t   db, 
		  char     * name)
{
    hdb_app_t       app      = NULL;
    wg_query      * query    = NULL;
    wg_query_arg    arglist[2];    
 
    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_APP);    

    arglist[1].column = HDB_APP_NAME;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_str(db, name, NULL);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    app = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return app;
}




static hobbes_id_t
__create_app_record(hdb_db_t      db,
		    char        * name, 
		    hobbes_id_t   enclave_id)
{
    void       * hdr_rec       = NULL;
    hobbes_id_t  app_id        = HOBBES_INVALID_ID;
    uint32_t     app_cnt       = 0;
    char         auto_name[32] = {[0 ... 31] = 0};

    hdb_app_t app   = NULL;

    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_APP_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("malformed database. Missing app Header\n");
	return HOBBES_INVALID_ID;
    }
    
    /* Get Next Available app ID and app count */
    app_id  = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_APP_HDR_NEXT));
    app_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_APP_HDR_CNT));
    
    if (name == NULL) {
	snprintf(auto_name, 31, "app-%d", app_id);
	name = auto_name;
    }
    
    /* Insert app into the db */
    app = wg_create_record(db, 5);
    wg_set_field(db, app, HDB_TYPE_FIELD,   wg_encode_int(db, HDB_REC_APP));
    wg_set_field(db, app, HDB_APP_ID,       wg_encode_int(db, app_id));
    wg_set_field(db, app, HDB_APP_STATE,    wg_encode_int(db, APP_INITTED));
    wg_set_field(db, app, HDB_APP_NAME,     wg_encode_str(db, name, NULL));
    wg_set_field(db, app, HDB_APP_ENCLAVE,  wg_encode_int(db, enclave_id));

    
    /* Update the app Header information */
    wg_set_field(db, hdr_rec, HDB_APP_HDR_NEXT, wg_encode_int(db, app_id  + 1));
    wg_set_field(db, hdr_rec, HDB_APP_HDR_CNT,  wg_encode_int(db, app_cnt + 1));

    return app_id;
}

hobbes_id_t 
hdb_create_app(hdb_db_t    db,
	       char      * name, 
	       hobbes_id_t enclave_id)
{
    wg_int      lock_id;
    hobbes_id_t app_id = HOBBES_INVALID_ID;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    app_id = __create_app_record(db, name, enclave_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }
    

    return app_id;
}


static int
__delete_app(hdb_db_t    db,
		 hobbes_id_t app_id)
{
    uint32_t      app_cnt = 0;
    void        * hdr_rec = NULL;
    hdb_app_t     app     = NULL;


    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_APP_HDR, NULL);
    
    if (!hdr_rec) {
	ERROR("Malformed database. Missing app Header\n");
	return -1;
    }
    
    app = __get_app_by_id(db, app_id);
  
    if (!app) {
	ERROR("Could not find app (id: %d)\n", app_id);
	return -1;
    }

    if (wg_delete_record(db, app) != 0) {
	ERROR("Could not delete app from database\n");
	return -1;
    }

    app_cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_APP_HDR_CNT));
    wg_set_field(db, hdr_rec, HDB_APP_HDR_CNT, wg_encode_int(db, app_cnt - 1));

    return 0;
}



int 
hdb_delete_app(hdb_db_t    db,
	       hobbes_id_t app_id)
{
    wg_int lock_id;
    int    ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __delete_app(db, app_id);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}


static hobbes_id_t 
__get_app_enclave(hdb_db_t    db,
		  hobbes_id_t app_id)
{
    hdb_app_t     app        = NULL;
    hobbes_id_t   enclave_id = HOBBES_INVALID_ID;


    app = __get_app_by_id(db, app_id);
    
    if (app == NULL) {
	ERROR("Could not find app (id: %d)\n", app_id);
	return HOBBES_INVALID_ID;
    }

    enclave_id = wg_decode_int(db, wg_get_field(db, app, HDB_APP_ENCLAVE));

    return enclave_id;
}

hobbes_id_t
hdb_get_app_enclave(hdb_db_t    db,
		    hobbes_id_t app_id)
{
    wg_int      lock_id;
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

    enclave_id = __get_app_enclave(db, app_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }
    

    return enclave_id;
}

static app_state_t
__get_app_state(hdb_db_t    db, 
		hobbes_id_t app_id)
{
    hdb_app_t   app   = NULL;
    app_state_t state = APP_ERROR;

    app = __get_app_by_id(db, app_id);
    
    if (app == NULL) {
	ERROR("Could not find app (id: %d)\n", app_id);
	return APP_ERROR;
    }

    state = wg_decode_int(db, wg_get_field(db, app, HDB_APP_STATE));

    return state;
}

app_state_t
hdb_get_app_state(hdb_db_t    db, 
		  hobbes_id_t app_id)
{
    wg_int lock_id;
    app_state_t state = 0;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return APP_ERROR;
    }

    state = __get_app_state(db, app_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return APP_ERROR;
    }
    

    return state;
}


static int
__set_app_state(hdb_db_t        db, 
		hobbes_id_t     app_id, 
		app_state_t state)
{
    hdb_app_t app = NULL;

    app = __get_app_by_id(db, app_id);
    
    if (app == NULL) {
	ERROR("Could not find app (id: %d)\n", app_id);
	return -1;
    }

    wg_set_field(db, app, HDB_APP_STATE, wg_encode_int(db, state));

    return 0;
}

int
hdb_set_app_state(hdb_db_t        db, 
		  hobbes_id_t     app_id, 
		  app_state_t state)
{
    wg_int lock_id;
    int ret = 0;

    lock_id = wg_start_write(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __set_app_state(db, app_id, state);
    
    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }
    

    return ret;
}



static char *
__get_app_name(hdb_db_t    db, 
	       hobbes_id_t app_id)
{
    hdb_app_t app = NULL;

    char * name = NULL;

    app = __get_app_by_id(db, app_id);
    
    if (app == NULL) {
	ERROR("Could not find app (id: %d)\n", app_id);
	return NULL;
    }

    name = wg_decode_str(db, wg_get_field(db, app, HDB_APP_NAME));

    return name;
}

char * 
hdb_get_app_name(hdb_db_t    db, 
		 hobbes_id_t app_id)
{
    wg_int lock_id;
    char * name = NULL;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

   name = __get_app_name(db, app_id);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return NULL;
    }
    
    return name;
}




static hobbes_id_t
__get_app_id(hdb_db_t   db, 
	     char     * app_name)
{
    hdb_app_t   app    = NULL;
    hobbes_id_t app_id = HOBBES_INVALID_ID;

    app = __get_app_by_name(db, app_name);
    
    if (app == NULL) {
	ERROR("Could not find app (name: %s)\n", app_name);
	return HOBBES_INVALID_ID;
    }

    app_id = wg_decode_int(db, wg_get_field(db, app, HDB_APP_ID));

    return app_id;
}

hobbes_id_t
hdb_get_app_id(hdb_db_t   db, 
	       char     * app_name)
{
    wg_int      lock_id;
    hobbes_id_t app_id = HOBBES_INVALID_ID;

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return HOBBES_INVALID_ID;
    }

   app_id = __get_app_id(db, app_name);
    
    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return HOBBES_INVALID_ID;
    }
    
    return app_id;
}

static hobbes_id_t *
__get_apps(hdb_db_t   db,
	   int      * num_apps)
{
    hobbes_id_t * id_arr  = NULL;
    void        * db_rec  = NULL;
    void        * hdr_rec = NULL;
    int           cnt     = 0;
    int           i       = 0;
    
    hdr_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_APP_HDR, NULL);    

    if (!hdr_rec) {
	ERROR("Malformed database. Missing app Header\n");
	return NULL;
    }

    cnt = wg_decode_int(db, wg_get_field(db, hdr_rec, HDB_APP_HDR_CNT));

    if (cnt == 0) {
	*num_apps = 0;
	return NULL;
    }

    id_arr = calloc(sizeof(hobbes_id_t), cnt);

    for (i = 0; i < cnt; i++) {
	db_rec = wg_find_record_int(db, HDB_TYPE_FIELD, WG_COND_EQUAL, HDB_REC_APP, db_rec);
	

	if (!db_rec) {
	    ERROR("Application Header state mismatch\n");
	    cnt = i;
	    break;
	}

	id_arr[i] = wg_decode_int(db, wg_get_field(db, db_rec, HDB_APP_ID));

    }

    *num_apps = cnt;
    return id_arr;
}


hobbes_id_t * 
hdb_get_apps(hdb_db_t   db,
	     int      * num_apps)
{
    hobbes_id_t * id_arr = NULL;
    wg_int        lock_id;

    if (!num_apps) {
	return NULL;
    }

    lock_id = wg_start_read(db);

    if (!lock_id) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    id_arr = __get_apps(db, num_apps);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Catastrophic database locking error\n");
	return NULL;
    }

    return id_arr;
}




/*
 * PMI Key Value Store
 */


/* This assumes the database lock is held */
static int
__put_pmi_keyval(hdb_db_t        db,
		 int             appid,
		 const char *    kvsname,
		 const char *    key,
		 const char *    val)
{
    void * rec = NULL;

    /* Insert the PMI Key/Value into the database */
    rec = wg_create_record(db, 5);
    wg_set_field(db, rec, HDB_TYPE_FIELD,            wg_encode_int(db, HDB_REC_PMI_KEYVAL));
    wg_set_field(db, rec, HDB_PMI_KVS_ENTRY_APPID,   wg_encode_int(db, appid));
    wg_set_field(db, rec, HDB_PMI_KVS_ENTRY_KVSNAME, wg_encode_str(db, (char *)kvsname, NULL));
    wg_set_field(db, rec, HDB_PMI_KVS_ENTRY_KEY,     wg_encode_str(db, (char *)key, NULL));
    wg_set_field(db, rec, HDB_PMI_KVS_ENTRY_VALUE,   wg_encode_str(db, (char *)val, NULL));

    return 0;
}


int
hdb_put_pmi_keyval(hdb_db_t      db,
		   int           appid,
		   const char *  kvsname,
		   const char *  key,
		   const char *  val)
{
    wg_int lock_id;
    int    ret;

    lock_id = wg_start_write(db);
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __put_pmi_keyval(db, appid, kvsname, key, val);

    if (!wg_end_write(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret;
}


static int
__get_pmi_keyval(hdb_db_t        db,
		 int             appid,
		 const char *    kvsname,
		 const char *    key,
		 const char **   val)
{
    hdb_pmi_keyval_t kvs_entry = NULL;
    wg_query *       query     = NULL;
    wg_query_arg     arglist[4];
    int              ret = -1;

    /* Build a database query to lookup the key */
    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_PMI_KEYVAL);

    arglist[1].column = HDB_PMI_KVS_ENTRY_APPID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, appid);

    arglist[2].column = HDB_PMI_KVS_ENTRY_KVSNAME;
    arglist[2].cond   = WG_COND_EQUAL;
    arglist[2].value  = wg_encode_query_param_str(db, (char *)kvsname, NULL);

    arglist[3].column = HDB_PMI_KVS_ENTRY_KEY;
    arglist[3].cond   = WG_COND_EQUAL;
    arglist[3].value  = wg_encode_query_param_str(db, (char *)key, NULL);

    /* Execute the query */
    query = wg_make_query(db, NULL, 0, arglist, 4);
    kvs_entry = wg_fetch(db, query);

    /* If the query succeeded, decode the value string */
    if (kvs_entry) {
	*val = wg_decode_str(db, wg_get_field(db, kvs_entry, HDB_PMI_KVS_ENTRY_VALUE));
	ret = 0;
    }

    /* Free memory */
    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);
    wg_free_query_param(db, arglist[2].value);
    wg_free_query_param(db, arglist[3].value);

    return ret;
}


int
hdb_get_pmi_keyval(hdb_db_t      db,
		   int           appid,
		   const char *  kvsname,
		   const char *  key,
		   const char ** val)
{
    wg_int lock_id;
    int    ret;

    lock_id = wg_start_read(db);
    if (!lock_id) {
	ERROR("Could not lock database\n");
	return -1;
    }

    ret = __get_pmi_keyval(db, appid, kvsname, key, val);

    if (!wg_end_read(db, lock_id)) {
	ERROR("Apparently this is catastrophic...\n");
	return -1;
    }

    return ret;
}

/******* Start of new stuff */

static hdb_pmi_barrier_t
__get_pmi_barrier(hdb_db_t  db,
                  int       appid)
{
    hdb_pmi_barrier_t         barrier_entry = NULL;
    wg_query                * query         = NULL;
    wg_query_arg              arglist[2];

    arglist[0].column = HDB_TYPE_FIELD;
    arglist[0].cond   = WG_COND_EQUAL;
    arglist[0].value  = wg_encode_query_param_int(db, HDB_REC_PMI_BARRIER);

    arglist[1].column = HDB_PMI_BARRIER_APPID;
    arglist[1].cond   = WG_COND_EQUAL;
    arglist[1].value  = wg_encode_query_param_int(db, appid);

    query = wg_make_query(db, NULL, 0, arglist, 2);

    barrier_entry = wg_fetch(db, query);

    wg_free_query(db, query);
    wg_free_query_param(db, arglist[0].value);
    wg_free_query_param(db, arglist[1].value);

    return barrier_entry;
}

int
hdb_create_pmi_barrier(hdb_db_t      db,
		       int           appid,
		       int           rank,
		       int           size,
		       xemem_segid_t segid)
{
    wg_int lock_id;
    void * rec = NULL;

    if ((lock_id = wg_start_write(db)) == 0) {
	ERROR("Could not lock database\n");
	return -1;
    }

    if ((rec = __get_pmi_barrier(db, appid)) == NULL) {
	/* Create the pmi barrier entry if no earlier process has created the pmi barrier entry */
	rec = wg_create_record(db, size+3);
	wg_set_field(db, rec, HDB_TYPE_FIELD, wg_encode_int(db, HDB_REC_PMI_BARRIER));
	wg_set_field(db, rec, HDB_PMI_BARRIER_APPID, wg_encode_int(db, appid));
	wg_set_field(db, rec, HDB_PMI_BARRIER_COUNTER, wg_encode_int(db, 0));
    }

    wg_set_field(db, rec, HDB_PMI_BARRIER_COUNTER+rank+1, wg_encode_int(db, segid));

    if (!(wg_end_write(db, lock_id))) {
	ERROR("Could not unlock database\n");
	return -1;
    }

    return 0;
}

int
hdb_pmi_barrier_increment(hdb_db_t db,
			  int      appid)
{
    wg_int lock_id;

    if ((lock_id = wg_start_write(db)) == 0) {
	ERROR("Could not lock database\n");
	return -1;
    }

    hdb_pmi_barrier_t barrier_entry = __get_pmi_barrier(db, appid);

    /* Increment the barrier counter */
    int count = wg_decode_int(db, wg_get_field(db, barrier_entry, HDB_PMI_BARRIER_COUNTER));
    count++;
    wg_set_field(db, barrier_entry, HDB_PMI_BARRIER_COUNTER, wg_encode_int(db, count));

    if (!(wg_end_write(db, lock_id))) {
	ERROR("Could not unlock database\n");
	return -1;
    }

    return count;
}

xemem_segid_t *
hdb_pmi_barrier_retire(hdb_db_t         db,
		       int              appid,
		       int              size)
{
    wg_int lock_id;

    if ((lock_id = wg_start_write(db)) == 0) {
	ERROR("Could not lock database\n");
	return NULL;
    }

    hdb_pmi_barrier_t barrier_entry = __get_pmi_barrier(db, appid);
    
    xemem_segid_t* segids = (xemem_segid_t*) calloc(sizeof(xemem_segid_t), size);

    int i;
    for(i=0; i<size; ++i) {
	segids[i] = wg_decode_int(db, wg_get_field(db, barrier_entry, HDB_PMI_BARRIER_COUNTER+i+1));
    }
    
    /* Reset the counter and wake up all other processes in the barrier */
    wg_set_field(db, barrier_entry, HDB_PMI_BARRIER_COUNTER, wg_encode_int(db, 0));

    if (!(wg_end_write(db, lock_id))) {
	ERROR("Could not unlock database\n");
	return NULL;
    }

    return segids;
}
