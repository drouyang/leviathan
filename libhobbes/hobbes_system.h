/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_SYSTEM_H__
#define __HOBBES_SYSTEM_H__

#include <stdint.h>


#include "hobbes.h"
#include "hobbes_cmd_queue.h"

#define HOBBES_INVALID_CPU_ID    (-1)
#define HOBBES_INVALID_ADDR      (-1)
#define HOBBES_INVALID_NUMA_ID   (-1)

typedef enum {
    MEMORY_INVALID,
    MEMORY_RSVD,
    MEMORY_FREE,
    MEMORY_ALLOCATED
} mem_state_t;

typedef enum {
    CPU_INVALID,
    CPU_RSVD,
    CPU_FREE,
    CPU_ALLOCATED
} cpu_state_t;



struct hobbes_system_info {
    uint64_t num_node_nodes;
    uint64_t num_cpus;
    uint64_t num_mem_blks;
    uint64_t mem_size;
    uint64_t free_mem;
    uint64_t free_cpus;
};


int hobbes_assign_memory(hcq_handle_t hcq,
			 uintptr_t    base_addr, 
			 uint64_t     size,
			 bool         allocated,
			 bool         zeroed);
			 

//int hobbes_register_cpu(

uint32_t  hobbes_get_numa_cnt(void);
uint64_t  hobbes_get_free_mem(void);
uint64_t  hobbes_get_mem_size(void);
uint64_t  hobbes_get_block_size(void);

struct hobbes_memory_info {
    uintptr_t   base_addr;
    uint64_t    size_in_bytes;
    uint32_t    numa_node;
    mem_state_t state;
    hobbes_id_t enclave_id;
    hobbes_id_t app_id;
};


struct hobbes_memory_info * 
hobbes_get_memory_list(uint64_t * num_mem_blks);

struct hobbes_cpu_info {
    uint32_t    cpu_id;
    uint32_t    numa_node;
    cpu_state_t state;
    hobbes_id_t enclave_id;
};

struct hobbes_cpu_info *
hobbes_get_cpu_list(int * num_cpus);

const char * mem_state_to_str(mem_state_t state);
const char * cpu_state_to_str(cpu_state_t state);

#endif
