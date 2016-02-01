 /* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_SYSTEM_H__
#define __HOBBES_SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>


#include "hobbes.h"
#include "hobbes_cmd_queue.h"

#define HOBBES_INVALID_CPU_ID    ((uint32_t)-1)
#define HOBBES_INVALID_ADDR      ((uint64_t)-1)
#define HOBBES_INVALID_NUMA_ID   ((uint32_t)-1)

#define HOBBES_ANY_NUMA_ID       ((uint32_t)-1)
#define HOBBES_ANY_CPU_ID        ((uint32_t)-1)

typedef enum {
    MEMORY_INVALID   = 0,
    MEMORY_RSVD      = 1,
    MEMORY_FREE      = 2,
    MEMORY_ALLOCATED = 3
} mem_state_t;

typedef enum {
    CPU_INVALID   = 0,
    CPU_RSVD      = 1,
    CPU_FREE      = 2,
    CPU_ALLOCATED = 3
} cpu_state_t;



struct hobbes_system_info {
    uint64_t num_node_nodes;
    uint64_t num_cpus;
    uint64_t num_mem_blks;
    uint64_t mem_size;
    uint64_t free_mem;
    uint64_t free_cpus;
};


uint32_t  hobbes_get_numa_cnt(void);


/* CPU allocation */

uint32_t
hobbes_alloc_cpu(hobbes_id_t enclave_id,
		 uint32_t    numa_node);


uint32_t
hobbes_alloc_specific_cpu(hobbes_id_t enclave_id,
			  uint32_t    cpu_id);

int 
hobbes_free_cpu(uint32_t cpu_id);


int 
hobbes_free_enclave_cpus(hobbes_id_t enclave_id);


/* Memory Allocation */


uintptr_t 
hobbes_alloc_mem(hobbes_id_t enclave_id,
		 int         numa_node,
		 uintptr_t   size_in_bytes);

int
hobbes_alloc_mem_regions(hobbes_id_t enclave_id,
			 int         numa_node,
			 uint32_t    num_regions,
			 uintptr_t   size_in_bytes,
			 uintptr_t * region_array);

uintptr_t 
hobbes_alloc_mem_block(hobbes_id_t enclave_id,
		       int         numa_node,
		       uint32_t    block_span);

int
hobbes_alloc_mem_blocks(hobbes_id_t enclave_id,
			int         numa_node,
			uint32_t    num_blocks,
			uint32_t    block_span,
			uintptr_t * block_array);

int 
hobbes_alloc_mem_addr(hobbes_id_t enclave_id,
		      uintptr_t   base_addr, 
		      uintptr_t   size_in_bytes);


int
hobbes_free_mem(uintptr_t addr, 
		uintptr_t size_in_bytes);



int
hobbes_free_mem_block(uintptr_t addr,
		      uint32_t  block_span);

int
hobbes_free_enclave_mem(hobbes_id_t enclave_id);





uint64_t  hobbes_get_free_mem(void);   /* Returns size in Bytes */
uint64_t  hobbes_get_mem_size(void);   /* Returns size in Bytes */
uint64_t  hobbes_get_block_size(void); /* Returns size in Bytes */






struct hobbes_memory_info {
    uintptr_t   base_addr;
    uint64_t    size_in_bytes;
    uint32_t    numa_node;
    mem_state_t state;
    hobbes_id_t enclave_id;
    hobbes_id_t app_id;
};

struct hobbes_cpu_info {
    uint32_t    cpu_id;
    uint32_t    apic_id;
    uint32_t    numa_node;
    cpu_state_t state;
    hobbes_id_t enclave_id;
};



struct hobbes_memory_info * 
hobbes_get_memory_list(uint64_t * num_mem_blks);

struct hobbes_cpu_info *
hobbes_get_cpu_list(uint32_t * num_cpus);

uint32_t
hobbes_get_cpu_apic_id(uint32_t cpu_id);

const char * mem_state_to_str(mem_state_t state);
const char * cpu_state_to_str(cpu_state_t state);






int 
hobbes_assign_memory(hobbes_id_t  enclave_id,
		     uintptr_t    base_addr, 
		     uint64_t     size,
		     bool         allocated,
		     bool         zeroed);

int
hobbes_remove_memory(hobbes_id_t enclave_id,
		     uintptr_t   base_addr,
		     uint64_t    size,
		     bool        allocated);

int
hobbes_assign_cpu(hobbes_id_t enclave_id,
		  uint32_t    cpu_id,
		  uint32_t    apic_id);


#ifdef __cplusplus
}
#endif

#endif
