/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_SYS_DB_H__
#define __HOBBES_SYS_DB_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "hobbes.h"
#include "hobbes_db.h"



/* 
 *  System Info
 */

int
hdb_init_system_info(hdb_db_t db,
		     uint32_t numa_nodes, 
		     uint64_t mem_blk_size);


uint32_t
hdb_get_sys_numa_cnt(hdb_db_t db);

uint64_t 
hdb_get_sys_free_blk_cnt(hdb_db_t db);

uint64_t 
hdb_get_sys_blk_cnt(hdb_db_t db);

uint64_t 
hdb_get_sys_blk_size(hdb_db_t db);


/* CPU Info */

int 
hdb_register_cpu(hdb_db_t    db,
		 uint32_t    cpu_id,
		 uint32_t    numa_node,
		 cpu_state_t state,
		 hobbes_id_t enclave_id);

uint32_t 
hdb_get_cpu_numa_node(hdb_db_t db,
		      uint32_t cpu_id);

cpu_state_t 
hdb_get_cpu_state(hdb_db_t db,
		  uint32_t cpu_id);

hobbes_id_t 
hdb_get_cpu_enclave_id(hdb_db_t db,
		       uint32_t cpu_id);


uint32_t 
hdb_alloc_cpu(hdb_db_t    db,
	      uint32_t    cpu_id,
	      uint32_t    numa_node,
	      hobbes_id_t enclave_id); 


int 
hdb_free_cpu(hdb_db_t db,
	     uint32_t cpu_id);


int 
hdb_free_enclave_cpus(hdb_db_t    db,
		      hobbes_id_t enclave_id);

uint32_t *
hdb_get_cpus(hdb_db_t   db,
	     uint32_t * num_cpus);



/* Memory Info */


int 
hdb_register_memory(hdb_db_t    db,
		    uintptr_t   base_addr,
		    uint64_t    blk_size,
		    uint32_t    numa_node,
		    mem_state_t state,
		    hobbes_id_t enclave_id);




int
hdb_alloc_blocks(hdb_db_t    db,
		 hobbes_id_t enclave_id, 
		 uint32_t    numa_node,
		 uint32_t    num_blocks, 
		 uint32_t    block_span,
		 uintptr_t * block_array);


uintptr_t 
hdb_alloc_block(hdb_db_t    db,
		hobbes_id_t enclave_id, 
		uint32_t    numa_node,
		uint32_t    block_span);

int
hdb_alloc_block_addr(hdb_db_t    db, 
		     hobbes_id_t enclave_id, 
		     uint32_t    block_span,
		     uintptr_t   base_addr);

int 
hdb_free_block(hdb_db_t  db,
	       uintptr_t base_addr, 
	       uint32_t  block_span);

int 
hdb_free_enclave_blocks(hdb_db_t    db,
			hobbes_id_t enclave_id);

uint32_t
hdb_get_mem_numa_node(hdb_db_t  db,
		      uintptr_t base_addr);

mem_state_t
hdb_get_mem_state(hdb_db_t  db,
		  uintptr_t base_addr);

hobbes_id_t 
hdb_get_mem_enclave_id(hdb_db_t  db,
		       uintptr_t base_addr);

hobbes_id_t 
hdb_get_mem_app_id(hdb_db_t  db,
		   uintptr_t base_addr);


uintptr_t * 
hdb_get_mem_blocks(hdb_db_t   db,
		   uint64_t * num_blks);



/* Debugging */

void 
hdb_sys_print_free_blks(hdb_db_t db);

#ifdef __cplusplus
}
#endif


#endif
