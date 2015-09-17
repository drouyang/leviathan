/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <pet_log.h>

#include <hobbes.h>
#include <hobbes_system.h>

int 
list_memory_main(int argc, char ** argv)
{
    struct hobbes_memory_info * blk_arr = NULL;
    uint32_t num_numa  = 0;
    uint64_t num_blks  = 0;
    uint64_t blk_size  = 0;

    uint64_t total_mem = 0;
    uint64_t free_mem  = 0;

    uint64_t i = 0;
    


    num_numa  = hobbes_get_numa_cnt();
    total_mem = hobbes_get_mem_size();
    free_mem  = hobbes_get_free_mem();
    blk_size  = hobbes_get_block_size();

    blk_arr   = hobbes_get_memory_list(&num_blks);


    if (num_blks == -1) {
	ERROR("Could not retrieve memory list\n");
	return -1;
    }


    printf("%lu memory blocks [block size=%luMB] (%d numa domains)\n", num_blks, blk_size / (1024 * 1024), num_numa);
    printf("Total mem: %luMB ; Free Mem: %luMB\n", 
	   total_mem / (1024 * 1024), 
	   free_mem  / (1024 * 1024));

    if (num_blks == 0) {
	return 0;
    }

    printf("-----------------------------------------------------------------------------\n");
    printf("| Blk ID | Physical Address   | State      | Numa | Enclave ID | App ID     |\n");
    printf("-----------------------------------------------------------------------------\n");

    for (i = 0; i < num_blks; i++) {
	printf("| %-*lu | 0x%0*lx | %-*s | %-*d | %-*d | %-*d |\n",
	       6,  i,
               16, blk_arr[i].base_addr,
	       10, mem_state_to_str(blk_arr[i].state),
	       4,  blk_arr[i].numa_node,
               10, blk_arr[i].enclave_id,
               10, blk_arr[i].app_id);
    }

    printf("-----------------------------------------------------------------------------\n");
    
    free(blk_arr);

    return 0;
}



int 
list_cpus_main(int argc, char ** argv)
{

    return 0;
}
