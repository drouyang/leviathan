/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */


#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes.h"
#include "hobbes_util.h"
#include "hobbes_cmd_queue.h"
#include "hobbes_sys_db.h"
#include "hobbes_db.h"


extern hdb_db_t hobbes_master_db;



uint32_t 
hobbes_get_numa_cnt(void)
{
    return hdb_get_sys_numa_cnt(hobbes_master_db);
}

uint64_t
hobbes_get_block_size(void)
{
    return hdb_get_sys_blk_size(hobbes_master_db);
}

uint64_t 
hobbes_get_mem_size(void)
{
    uint64_t blk_cnt  = 0;
    uint64_t blk_size = 0;

    blk_cnt  = hdb_get_sys_blk_cnt  ( hobbes_master_db );
    blk_size = hdb_get_sys_blk_size ( hobbes_master_db );

    if ((blk_cnt == (uint64_t) 0) || (blk_size == (uint64_t) 0) ||
	(blk_cnt == (uint64_t)-1) || (blk_size == (uint64_t)-1)) {
	return 0;
    }

    return blk_cnt * blk_size;
}

uint64_t 
hobbes_get_free_mem(void)
{
    uint64_t blk_cnt  = 0;
    uint64_t blk_size = 0;

    blk_cnt  = hdb_get_sys_free_blk_cnt  ( hobbes_master_db );
    blk_size = hdb_get_sys_blk_size      ( hobbes_master_db );

    if ((blk_cnt == (uint64_t) 0) || (blk_size == (uint64_t) 0) ||
	(blk_cnt == (uint64_t)-1) || (blk_size == (uint64_t)-1)) {
	return 0;
    }

    return blk_cnt * blk_size;
}





uintptr_t 
hobbes_alloc_mem(hobbes_id_t enclave_id,
		 int         numa_node, 
		 uintptr_t   size_in_bytes)
{
    uint32_t block_span = ( ((size_in_bytes / hobbes_get_block_size())     ) +
			    ((size_in_bytes % hobbes_get_block_size()) != 0) );

    return hobbes_alloc_mem_block(enclave_id, numa_node, block_span);
}


uintptr_t 
hobbes_alloc_mem_block(hobbes_id_t enclave_id,
		       int         numa_node,
		       uint32_t    block_span)
{
    uintptr_t block_paddr = 0;

    printf("Allocating %d blocks of memory\n", block_span);

    block_paddr = hdb_alloc_block(hobbes_master_db, enclave_id, numa_node, block_span);

    return block_paddr;
}

int 
hobbes_alloc_mem_regions(hobbes_id_t enclave_id,
			 int         numa_node,
			 uint32_t    num_regions,
			 uintptr_t   size_in_bytes,
			 uintptr_t * region_array)
{
    uint32_t block_span = ( ((size_in_bytes / hobbes_get_block_size())     ) +
			    ((size_in_bytes % hobbes_get_block_size()) != 0) );
    
    return hobbes_alloc_mem_blocks(enclave_id, numa_node, num_regions, block_span, region_array);
}


int 
hobbes_alloc_mem_blocks(hobbes_id_t enclave_id,
			int         numa_node,
			uint32_t    num_blocks,
			uint32_t    block_span,
			uintptr_t * block_array)
{
    return hdb_alloc_blocks(hobbes_master_db, enclave_id, numa_node, num_blocks, block_span, block_array);
}

int
hobbes_alloc_mem_addr(hobbes_id_t enclave_id, 
		      uintptr_t   base_addr,
		      uintptr_t   size_in_bytes)
{
    uint32_t block_span = ( ((size_in_bytes / hobbes_get_block_size())     ) +
			    ((size_in_bytes % hobbes_get_block_size()) != 0) );
    
    return hdb_alloc_block_addr(hobbes_master_db, enclave_id, block_span, base_addr);
}


int hobbes_free_mem_block(uintptr_t addr, 
			  uint32_t  block_span)
{
    return hdb_free_block(hobbes_master_db, addr, block_span);
}

int 
hobbes_free_mem(uintptr_t addr,
		uintptr_t size_in_bytes)
{
    uint32_t block_span = ( ((size_in_bytes / hobbes_get_block_size())     ) +
			    ((size_in_bytes % hobbes_get_block_size()) != 0) );

    return hobbes_free_mem_block(addr, block_span);
}

int
hobbes_free_enclave_mem(hobbes_id_t enclave_id)
{
    return hdb_free_enclave_blocks(hobbes_master_db, enclave_id);
}



/****
 * CPU
 ****/

uint32_t
hobbes_alloc_cpu(hobbes_id_t enclave_id,
		 uint32_t    numa_node)
{
    return hdb_alloc_cpu(hobbes_master_db, HOBBES_ANY_CPU_ID, numa_node, enclave_id);
}


uint32_t 
hobbes_alloc_specific_cpu(hobbes_id_t enclave_id,
			  uint32_t    cpu_id)
{
    return hdb_alloc_cpu(hobbes_master_db, cpu_id, HOBBES_ANY_NUMA_ID, enclave_id);
}

int 
hobbes_free_cpu(uint32_t cpu_id)
{
    return hdb_free_cpu(hobbes_master_db, cpu_id);
}


int 
hobbes_free_enclave_cpus(hobbes_id_t enclave_id)
{
    return hdb_free_enclave_cpus(hobbes_master_db, enclave_id);
}


static struct hobbes_memory_info *
__get_memory_list(hobbes_id_t  enclave_id,
		  uint64_t   * num_mem_blks)
{
    struct hobbes_memory_info * blk_arr  = NULL;
    uint64_t                  * addr_arr = NULL;
    
    uint64_t blk_cnt = 0;
    uint64_t i       = 0;

    if (enclave_id == HOBBES_INVALID_ID)
	addr_arr = hdb_get_mem_blocks(hobbes_master_db, &blk_cnt);
    else
	addr_arr = hdb_get_enclave_mem_blocks(hobbes_master_db, enclave_id, &blk_cnt);

    if (addr_arr == NULL) {
	ERROR("Could not retrieve memory block list\n");
	return NULL;
    }
    
    blk_arr = calloc(sizeof(struct hobbes_memory_info), blk_cnt);
    
    for (i = 0; i < blk_cnt; i++) {
	blk_arr[i].base_addr = addr_arr[i];
	
	blk_arr[i].size_in_bytes = hdb_get_sys_blk_size(hobbes_master_db);

	blk_arr[i].numa_node     = hdb_get_mem_numa_node  ( hobbes_master_db, addr_arr[i] );
	blk_arr[i].state         = hdb_get_mem_state      ( hobbes_master_db, addr_arr[i] );
	blk_arr[i].enclave_id    = hdb_get_mem_enclave_id ( hobbes_master_db, addr_arr[i] );
	blk_arr[i].app_id        = hdb_get_mem_app_id     ( hobbes_master_db, addr_arr[i] );
    }

    free(addr_arr);

    *num_mem_blks = blk_cnt;

    return blk_arr;

}

struct hobbes_memory_info * 
hobbes_get_memory_list(uint64_t * num_mem_blks)
{
    return __get_memory_list(HOBBES_INVALID_ID, num_mem_blks);
}


struct hobbes_memory_info *
hobbes_get_enclave_memory_list(hobbes_id_t enclave_id,
			       uint64_t  * num_mem_blks)
{
    return __get_memory_list(enclave_id, num_mem_blks);
}

struct hobbes_cpu_info *
hobbes_get_cpu_list(uint32_t * num_cpus)
{
    struct hobbes_cpu_info * cpu_arr = NULL;
    uint32_t               * id_arr  = NULL;

    uint32_t cpu_cnt = 0;
    uint32_t i       = 0;

    id_arr = hdb_get_cpus(hobbes_master_db, &cpu_cnt);
    *num_cpus = cpu_cnt;

    if (id_arr == NULL) {
	ERROR("Could not retrieve CPU list\n");
	return NULL;
    }

    cpu_arr = calloc(sizeof(struct hobbes_cpu_info), cpu_cnt);

    for (i = 0; i < cpu_cnt; i++) {
	cpu_arr[i].cpu_id	      = id_arr[i];

	cpu_arr[i].apic_id	      = hdb_get_cpu_apic_id	       ( hobbes_master_db, id_arr[i] );
	cpu_arr[i].numa_node	      = hdb_get_cpu_numa_node	       ( hobbes_master_db, id_arr[i] );
	cpu_arr[i].state	      = hdb_get_cpu_state	       ( hobbes_master_db, id_arr[i] );
	cpu_arr[i].enclave_id	      = hdb_get_cpu_enclave_id	       ( hobbes_master_db, id_arr[i] );
	cpu_arr[i].enclave_logical_id = hdb_get_cpu_enclave_logical_id ( hobbes_master_db, id_arr[i] );
    }

    free(id_arr);

    return cpu_arr;
}

uint32_t
hobbes_get_cpu_apic_id(uint32_t cpu_id)
{
    return hdb_get_cpu_apic_id(hobbes_master_db, cpu_id);
}

int
hobbes_set_cpu_enclave_logical_id(uint32_t cpu_id,
				  uint32_t logical_id)
{
    return hdb_set_cpu_enclave_logical_id(hobbes_master_db, cpu_id, logical_id);
}


const char * 
mem_state_to_str(mem_state_t state) 
{
    
    switch (state) {
	case MEMORY_INVALID:   return "INVALID";
	case MEMORY_RSVD:      return "RSVD";
	case MEMORY_FREE:      return "FREE";
	case MEMORY_ALLOCATED: return "ALLOCATED";

	default: return NULL;
    }

    return NULL;
}


const char * 
cpu_state_to_str(cpu_state_t state) 
{
    switch (state) {
	case CPU_INVALID:   return "INVALID";
	case CPU_RSVD:      return "RSVD";
	case CPU_FREE:      return "FREE";
	case CPU_ALLOCATED: return "ALLOCATED";

	default: return NULL;
    }

    return NULL;
}



int
hobbes_assign_cpu(hobbes_id_t enclave_id,
		  uint32_t    cpu_id,
		  uint32_t    apic_id)
{
    hcq_handle_t hcq   = HCQ_INVALID_HANDLE;

    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    pet_xml_t cmd_xml  = PET_INVALID_XML;

    uint32_t  ret_size =  0;
    uint8_t * ret_data =  NULL;

    char    * tmp_str  =  NULL;
    int       ret      = -1;

    
    hcq = hobbes_open_enclave_cmdq(enclave_id);

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not connect to enclave's command queue\n");
	goto err;
    }

    cmd_xml = pet_xml_new_tree("cpus");

    if (cmd_xml == PET_INVALID_XML) {
        ERROR("Could not create xml command\n");
        goto err;
    }

    /* Cpu ID */
    if (asprintf(&tmp_str, "%u", cpu_id) == -1) {
	tmp_str = NULL;
	goto err;
    }
    pet_xml_add_val(cmd_xml, "phys_cpu_id",  tmp_str);
    smart_free(tmp_str);

    /* Apic ID */
    if (asprintf(&tmp_str, "%u", apic_id) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "apic_id",  tmp_str);
    smart_free(tmp_str);
    
    tmp_str = pet_xml_get_str(cmd_xml);
    cmd     = hcq_cmd_issue(hcq, HOBBES_CMD_ADD_CPU, strlen(tmp_str) + 1, tmp_str);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Error issuing add memory command (%s)\n", tmp_str);
	goto err;
    } 


    ret = hcq_get_ret_code(hcq, cmd);
    
    if (ret != 0) {
	ret_data = hcq_get_ret_data(hcq, cmd, &ret_size);
	ERROR("Error adding cpu (%s) [ret=%d]\n", ret_data, ret);
	goto err;
    }

    hcq_cmd_complete(hcq, cmd);

    hcq_disconnect(hcq);

    smart_free(tmp_str);
    pet_xml_free(cmd_xml);

    return ret;

 err:
    if (tmp_str != NULL)               smart_free(tmp_str);                 
    if (cmd_xml != PET_INVALID_XML)    pet_xml_free(cmd_xml);
    if (cmd     != HCQ_INVALID_CMD)    hcq_cmd_complete(hcq, cmd);
    if (hcq     != HCQ_INVALID_HANDLE) hcq_disconnect(hcq);
    return -1;

    return -1;
}



int 
hobbes_assign_memory(hobbes_id_t  enclave_id,
		     uintptr_t    base_addr, 
		     uint64_t     size,
		     bool         allocated,
		     bool         zeroed)
{
    hcq_handle_t hcq   = HCQ_INVALID_HANDLE;

    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    pet_xml_t cmd_xml  = PET_INVALID_XML;

    uint32_t  ret_size =  0;
    uint8_t * ret_data =  NULL;

    char    * tmp_str  =  NULL;
    int       ret      = -1;

    
    hcq = hobbes_open_enclave_cmdq(enclave_id);

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not connect to enclave's command queue\n");
	goto err;
    }

    cmd_xml = pet_xml_new_tree("memory");

    if (cmd_xml == PET_INVALID_XML) {
        ERROR("Could not create xml command\n");
        goto err;
    }

    /* Base Address */
    if (asprintf(&tmp_str, "%p", (void *)base_addr) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "base_addr",  tmp_str);
    smart_free(tmp_str);

    /* Size */
    if (asprintf(&tmp_str, "%lu", size) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "size",  tmp_str);
    smart_free(tmp_str);
    
    pet_xml_add_val(cmd_xml, "allocated", (allocated) ? "1" : "0");
    pet_xml_add_val(cmd_xml, "zeroed",    (zeroed)    ? "1" : "0");

    tmp_str = pet_xml_get_str(cmd_xml);
    cmd     = hcq_cmd_issue(hcq, HOBBES_CMD_ADD_MEM, strlen(tmp_str) + 1, tmp_str);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Error issuing add memory command (%s)\n", tmp_str);
	goto err;
    } 


    ret = hcq_get_ret_code(hcq, cmd);
    
    if (ret != 0) {
	ret_data = hcq_get_ret_data(hcq, cmd, &ret_size);
	ERROR("Error adding memory (%s) [ret=%d]\n", ret_data, ret);
	goto err;
    }

    hcq_cmd_complete(hcq, cmd);

    hcq_disconnect(hcq);

    smart_free(tmp_str);
    pet_xml_free(cmd_xml);

    return ret;

 err:
    if (tmp_str != NULL)               smart_free(tmp_str);                 
    if (cmd_xml != PET_INVALID_XML)    pet_xml_free(cmd_xml);
    if (cmd     != HCQ_INVALID_CMD)    hcq_cmd_complete(hcq, cmd);
    if (hcq     != HCQ_INVALID_HANDLE) hcq_disconnect(hcq);
    return -1;
}

int 
hobbes_remove_memory(hobbes_id_t  enclave_id,
		     uintptr_t    base_addr, 
		     uint64_t     size,
		     bool         allocated)
{
    hcq_handle_t hcq   = HCQ_INVALID_HANDLE;

    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    pet_xml_t cmd_xml  = PET_INVALID_XML;

    uint32_t  ret_size =  0;
    uint8_t * ret_data =  NULL;

    char    * tmp_str  =  NULL;
    int       ret      = -1;

    
    hcq = hobbes_open_enclave_cmdq(enclave_id);

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not connect to enclave's command queue\n");
	goto err;
    }

    cmd_xml = pet_xml_new_tree("memory");

    if (cmd_xml == PET_INVALID_XML) {
        ERROR("Could not create xml command\n");
        goto err;
    }

    /* Base Address */
    if (asprintf(&tmp_str, "%p", (void *)base_addr) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "base_addr",  tmp_str);
    smart_free(tmp_str);

    /* Size */
    if (asprintf(&tmp_str, "%lu", size) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "size",  tmp_str);
    smart_free(tmp_str);

    pet_xml_add_val(cmd_xml, "allocated", (allocated) ? "1" : "0");
    
    tmp_str = pet_xml_get_str(cmd_xml);
    cmd     = hcq_cmd_issue(hcq, HOBBES_CMD_REMOVE_MEM, strlen(tmp_str) + 1, tmp_str);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Error issuing remove memory command (%s)\n", tmp_str);
	goto err;
    } 


    ret = hcq_get_ret_code(hcq, cmd);
    
    if (ret != 0) {
	ret_data = hcq_get_ret_data(hcq, cmd, &ret_size);
	ERROR("Error removing memory (%s) [ret=%d]\n", ret_data, ret);
	goto err;
    }

    hcq_cmd_complete(hcq, cmd);

    hcq_disconnect(hcq);

    smart_free(tmp_str);
    pet_xml_free(cmd_xml);

    return ret;

 err:
    if (tmp_str != NULL)               smart_free(tmp_str);                 
    if (cmd_xml != PET_INVALID_XML)    pet_xml_free(cmd_xml);
    if (cmd     != HCQ_INVALID_CMD)    hcq_cmd_complete(hcq, cmd);
    if (hcq     != HCQ_INVALID_HANDLE) hcq_disconnect(hcq);
    return -1;
}
