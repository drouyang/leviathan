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
#include <getopt.h>

#include <pet_log.h>

#include <hobbes.h>
#include <hobbes_system.h>
#include <hobbes_sys_db.h>
#include <hobbes_util.h>

#include <pisces_ctrl.h>

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


    if (blk_arr == NULL) {
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
    struct hobbes_cpu_info * cpu_arr = NULL;
    uint32_t num_numa  = 0;
    uint32_t num_cpus  = 0;

    uint32_t i = 0;
    
    num_numa  = hobbes_get_numa_cnt();
    cpu_arr   = hobbes_get_cpu_list(&num_cpus);

    if (cpu_arr == NULL) {
	ERROR("Could not retrieve memory list\n");
	return -1;
    }


    printf("%u CPUs (%d numa domains)\n", num_cpus, num_numa);

    if (num_cpus == 0) {
	return 0;
    }

    printf("------------------------------------------------------\n");
    printf("| CPU ID |  APIC ID | State      | Numa | Enclave ID |\n");
    printf("------------------------------------------------------\n");

    for (i = 0; i < num_cpus; i++) {
	printf("| %-*u | %-*u | %-*s | %-*d | %-*d |\n",
               6,  cpu_arr[i].cpu_id,
               7,  cpu_arr[i].apic_id,
	       11, cpu_state_to_str(cpu_arr[i].state),
	       4,  cpu_arr[i].numa_node,
               10, cpu_arr[i].enclave_id);
    }

    printf("------------------------------------------------------\n");
    
    free(cpu_arr);

    return 0;
}


static void
assign_memory_usage(char * exec_name)
{
    printf("Usage: %s [options] <enclave name> <size (MB)>\n"
	"[-n, --numa=<numa zone>]\n"
	"[-c, --contiguous]\n",
	exec_name
    );
}


static int
__assign_memory_allow_discontiguous(hobbes_id_t enclave_id,
	 			    char      * enclave_name,
	 			    uint64_t    mem_size_bytes,
				    uint32_t    numa_node,
				    uint64_t  * bytes_assigned)
{
    uint64_t    block_size     = 0;
    uint32_t    num_regions    = 0;
    uint32_t    region_off     = 0;
    uintptr_t * regions        = NULL;
    int         status         = 0;

    *bytes_assigned = 0;

    block_size  = hobbes_get_block_size();
    num_regions = mem_size_bytes / block_size;

    regions = malloc(sizeof(uintptr_t) * num_regions);
    if (regions == NULL) {
	ERROR("Could not allocate region array\n");
	return -1;
    }

    /* Allocate the memory */
    status = hobbes_alloc_mem_regions(enclave_id, numa_node, num_regions, block_size, regions);
    if (status) {
	ERROR("Could not allocate %lu bytes of memory for enclave %s\n", mem_size_bytes, enclave_name);
	return -1;
    }

    /* Assign each region */
    for (region_off = 0; region_off < num_regions; region_off++) {
	status = hobbes_assign_memory(enclave_id, regions[region_off], block_size, false, true);
	if (status) {
	    ERROR("Could not assign memory to enclave %s\n", enclave_name);
	    hobbes_free_mem(regions[region_off], block_size);
	} else {
	    *bytes_assigned += block_size;
	}
    }

    free(regions);

    return 0;
}

static int
__assign_memory_contiguous(hobbes_id_t enclave_id,
			   char      * enclave_name,
			   uint64_t    mem_size_bytes,
			   uint32_t    numa_node,
			   uint64_t  * bytes_assigned)
{
    uintptr_t mem_addr = HOBBES_INVALID_ADDR;
    int       status   = 0;

    *bytes_assigned = 0;

    /* Allocate the memory */
    mem_addr = hobbes_alloc_mem(enclave_id, numa_node, mem_size_bytes);
    if (mem_addr == HOBBES_INVALID_ADDR) {
	ERROR("Could not allocate %lu bytes of memory for enclave %s\n", mem_size_bytes, enclave_name);
	return -1;
    }

    /* Assign the memory */
    status = hobbes_assign_memory(enclave_id, mem_addr, mem_size_bytes, false, true);
    if (status) {
	ERROR("Could not assign memory to enclave %s\n", enclave_name);
	hobbes_free_mem(mem_addr, mem_size_bytes);
	return -1;
    }

    *bytes_assigned = mem_size_bytes;

    return 0;
}

static int
assign_memory(char   * enclave_name,
	      uint64_t mem_size_MB,
	      uint32_t numa_node,
	      int      allow_discontiguous)
{  
    hobbes_id_t    enclave_id     = HOBBES_INVALID_ID;
    enclave_type_t enclave_type   = INVALID_ENCLAVE;
    uint64_t       mem_size_bytes = 0;
    uint64_t       bytes_assigned = 0;
    uint64_t       block_size     = 0;
    int            status         = 0;

    enclave_id = hobbes_get_enclave_id(enclave_name);
    if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Cannot assign memory to enclave %s: cannot find enclave id\n", enclave_name);
	return -1;
    }

    enclave_type = hobbes_get_enclave_type(enclave_id);
    if ((enclave_type == INVALID_ENCLAVE) || 
	(enclave_type == VM_ENCLAVE)) {
	ERROR("Cannot assign memory to enclave %s: type %s not currently supported\n", 
		enclave_name, enclave_type_to_str(enclave_type));
	return -1;
    }

    /* Round memory up to nearest block size */
    block_size      = hobbes_get_block_size();
    mem_size_bytes  = mem_size_MB * (1024 * 1024);

    if (mem_size_bytes % block_size) {
	printf("Rounding up to nearest 'hobbes_block_size' bytes (hobbes_block_size == %lu bytes (%lu MB)\n",
		block_size, (block_size / (1024 * 1024)));

	mem_size_bytes += block_size - (mem_size_bytes % block_size);
	mem_size_MB     = mem_size_bytes / (1024 * 1024);
    }

    if (allow_discontiguous)
	status = __assign_memory_allow_discontiguous(enclave_id, enclave_name, mem_size_bytes, numa_node,
			&bytes_assigned);
    else
	status = __assign_memory_contiguous(enclave_id, enclave_name, mem_size_bytes, numa_node,
			&bytes_assigned);

    if (status != 0) {
	ERROR("Failed to assign memory to enclave %s\n", enclave_name);
	return -1;
    }

    printf("Assigned %lu (%lu MB) of (%s) memory to enclave %s\n",
	mem_size_bytes,
	mem_size_MB,
	(allow_discontiguous) ? "possibly discontiguous" : "contiguous",
	enclave_name);

    return 0;
}

int
assign_memory_main(int argc, char ** argv)
{
    int      allow_discontiguous = 1;
    uint32_t numa_node		 = HOBBES_ANY_NUMA_ID;
    uint64_t mem_size_MB	 = HOBBES_INVALID_ADDR;
    char   * enclave_name        = NULL;

    /* Get command line options */
    {
	char c = 0;
	int  opt_index = 0;

	struct option long_options[] =
	{
	    {"numa",		required_argument, 0, 'n'},
	    {"contiguous",	no_argument,	   0, 'c'},
	    {0, 0, 0, 0}
	};

	while ((c = getopt_long_only(argc, argv, "n:c", long_options, &opt_index)) != -1) {
	    switch (c) {
		case 'n':
		    numa_node = smart_atou32(numa_node, optarg);
		    if (numa_node == (uint32_t)HOBBES_INVALID_NUMA_ID) {
			ERROR("Invalid NUMA argument (%s)\n", optarg);
			assign_memory_usage(argv[0]);
			return -1;
		    }

		    if (numa_node >= hobbes_get_numa_cnt()) {
			ERROR("Invalid NUMA zone. %d specified, but only %d zones present\n",
				numa_node, hobbes_get_numa_cnt());
			assign_memory_usage(argv[0]);
			return -1;
		    }

		    break;

		case 'c':
		    allow_discontiguous = 0;
		    break;

		case '?':
		    ERROR("Invalid option specified\n");
		    assign_memory_usage(argv[0]);
		    return -1;
	    }
	}

	if (optind != (argc - 2)) {
	    assign_memory_usage(argv[0]);
	    return -1;
	}

	enclave_name = argv[optind];
	mem_size_MB  = smart_atou64(mem_size_MB, argv[optind + 1]);	
	if (mem_size_MB == HOBBES_INVALID_ADDR) {
	    ERROR("Invalid mem_size_MB: %s\n", argv[optind + 1]);
	    assign_memory_usage(argv[0]);
	    return -1;
	}
    }

    return assign_memory(enclave_name, mem_size_MB, numa_node, allow_discontiguous);
}

static void
assign_cpus_usage(char * exec_name)
{
    printf("Usage: %s [options] <enclave name> <num cpus>\n"
	"[-n, --numa=<numa zone>]\n",
	exec_name
    );
}


/* FIXME: currently, we don't use hobbes_assign_cpu for Pisces because assigning
 * cpus must go through the legacy Pisces control channel. Hopefully we can
 * pull the control channel at some point and just use Pisces ioctls to
 * set/reset the secondary trampooline target while the notification operations
 * go through the Hobbes command queues
 */
static int
__assign_cpu_pisces(hobbes_id_t enclave_id,
		    uint32_t    cpu_id,
		    uint32_t    apic_id)
{
    int dev_id = hobbes_get_enclave_dev_id(enclave_id);
    return pisces_add_offline_cpu(dev_id, cpu_id);
}

static int
__assign_cpu_master(hobbes_id_t enclave_id,
		    uint32_t    cpu_id,
		    uint32_t    apic_id)
{
    return hobbes_assign_cpu(enclave_id, cpu_id, apic_id);
}

static int
assign_cpus(char   * enclave_name,
	    uint32_t num_cpus,
	    uint32_t numa_node)
{
    hobbes_id_t    enclave_id     = HOBBES_INVALID_ID;
    enclave_type_t enclave_type   = INVALID_ENCLAVE;
    uint32_t       cpu_off        = 0;
    uint32_t       cpu_id         = HOBBES_INVALID_CPU_ID;
    uint32_t       apic_id        = HOBBES_INVALID_CPU_ID;
    uint32_t       cpus_assigned  = 0;
    uint32_t     * cpu_ids        = NULL;
    int            status         = 0;

    enclave_id = hobbes_get_enclave_id(enclave_name);
    if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Cannot assign cpus to enclave %s: cannot find enclave id\n", enclave_name);
	return -1;
    }

    enclave_type = hobbes_get_enclave_type(enclave_id);
    if ((enclave_type == INVALID_ENCLAVE) || 
	(enclave_type == VM_ENCLAVE)) {
	ERROR("Cannot assign cpus to enclave %s: type %s not currently supported\n", 
		enclave_name, enclave_type_to_str(enclave_type));
	return -1;
    }

    cpu_ids = malloc(sizeof(uint32_t) * num_cpus);
    if (cpu_ids == NULL) {
	ERROR("Could not allocate array for cpus\n");
	return -1;
    }

    for (cpu_off = 0; cpu_off < num_cpus; cpu_off++) {
	cpu_id = hobbes_alloc_cpu(enclave_id, numa_node);
	if (cpu_id == HOBBES_INVALID_CPU_ID) {
	    ERROR("Cannot allocate cpu for enclave %s\n", enclave_name);
	    goto cpu_free;
	}

	cpu_ids[cpu_off] = cpu_id;
    }

    for (cpu_off = 0; cpu_off < num_cpus; cpu_off++) {
	cpu_id  = cpu_ids[cpu_off];
	apic_id = hobbes_get_cpu_apic_id(cpu_id);
	if (apic_id == (uint32_t)-1) {
	    ERROR("Cannot get apic id for cpu %d: cannot assign cpu to enclave %s\n",
		cpu_id, enclave_name);
	    hobbes_free_cpu(cpu_id);
	    continue;
	}

	/* Assign each cpu */
	if (enclave_type == PISCES_ENCLAVE)
	    status = __assign_cpu_pisces(enclave_id, cpu_id, apic_id);
	else
	    status = __assign_cpu_master(enclave_id, cpu_id, apic_id);

	if (status != 0) {
	    ERROR("Could not assign cpu to enclave %s\n", enclave_name);
	    hobbes_free_cpu(cpu_id);
	    continue;
	}

	cpus_assigned++;
    }

    free(cpu_ids);

    printf("Assigned %u cpus to enclave %s\n",
        cpus_assigned,
	enclave_name);

    return 0;

cpu_free:
    {
	uint32_t i = 0;

	for (i = 0; i < cpu_off; i++) {
	    cpu_id = cpu_ids[i];
	    hobbes_free_cpu(cpu_id);
	}
    }

    free(cpu_ids);

    return -1;
}

int
assign_cpus_main(int argc, char ** argv)
{
    uint32_t numa_node	  = HOBBES_ANY_NUMA_ID;
    uint32_t num_cpus	  =  0;
    char   * enclave_name = NULL;

    /* Get command line options */
    {
	char c = 0;
	int  opt_index = 0;

	struct option long_options[] =
	{
	    {"numa",	required_argument, 0, 'n'},
	    {0, 0, 0, 0}
	};

	while ((c = getopt_long_only(argc, argv, "n:", long_options, &opt_index)) != -1) {
	    switch (c) {
		case 'n':
		    numa_node = smart_atou32(numa_node, optarg);
		    if (numa_node == (uint32_t)HOBBES_INVALID_NUMA_ID) {
			ERROR("Invalid NUMA argument (%s)\n", optarg);
			assign_cpus_usage(argv[0]);
			return -1;
		    }

		    if (numa_node >= hobbes_get_numa_cnt()) {
			ERROR("Invalid NUMA zone. %d specified, but only %d zones present\n",
				numa_node, hobbes_get_numa_cnt());
			assign_cpus_usage(argv[0]);
			return -1;
		    }

		    break;

		case '?':
		    ERROR("Invalid option specified\n");
		    assign_cpus_usage(argv[0]);
		    return -1;
	    }
	}

	if (optind != (argc - 2)) {
	    assign_cpus_usage(argv[0]);
	    return -1;
	}

	enclave_name = argv[optind];
	num_cpus     = smart_atou32(num_cpus, argv[optind + 1]);
	if (num_cpus == 0) {
	    ERROR("Invalid number of cpus: %s\n", argv[optind + 1]);
	    assign_cpus_usage(argv[0]);
	    return -1;
	}
    }

    return assign_cpus(enclave_name, num_cpus, numa_node);
}
