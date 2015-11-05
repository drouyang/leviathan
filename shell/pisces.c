#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <stdint.h>

#include <pisces.h>
#include <pisces_ctrl.h>

#include <pet_mem.h>
#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes.h"
#include "hobbes_db.h"
#include "hobbes_util.h"
#include "pisces.h"


extern hdb_db_t hobbes_master_db;






static int 
add_cpu_to_pisces(int pisces_id, 
		  int cpu_id)
{
    int ret = -1;

    ret = pisces_add_offline_cpu(pisces_id, cpu_id);

    return ret;
}



	   


int 
pisces_enclave_create(pet_xml_t   xml, 
		      char      * name)
{
    int         pisces_id  = -1;
    hobbes_id_t enclave_id = -1;

    /* Add enclave to the Master DB */
    {
	char * enclave_name = name; 

	if (enclave_name == NULL) {
	    enclave_name = pet_xml_get_val(xml, "name");
	}

	enclave_id = hdb_create_enclave(hobbes_master_db, enclave_name, pisces_id, PISCES_ENCLAVE, 0);

	if (enclave_id == -1) {
	    ERROR("Could not create enclave in database\n");
	    return -1;
	}
    }


    /* Load Enclave OS files */
    {
	char * kern        = pet_xml_get_val(xml, "kernel");
	char * initrd      = pet_xml_get_val(xml, "init_task");
	char * cmdline     = pet_xml_get_val(xml, "cmd_line");
	char * tmp_cmdline = NULL;

	
	if ( (kern == NULL) || (initrd == NULL) ) {
	    ERROR("Must specify a kernel and init_task for a pisces_enclave\n");

	    hdb_delete_enclave(hobbes_master_db, enclave_id);

	    return -1;
	} 

	/* update command line to include appropriate environment variables */
	asprintf(&tmp_cmdline, "%s init_envp=\"%s=%d\"", cmdline, HOBBES_ENV_ENCLAVE_ID, enclave_id);

	pisces_id = pisces_load(kern, initrd, tmp_cmdline);

	free(tmp_cmdline);

	if (pisces_id < 0) {

	    hdb_delete_enclave(hobbes_master_db, enclave_id);

	    ERROR("Could not load Pisces Enclave\n");
	    ERROR("\tkernel  = %s\n", kern);
	    ERROR("\tinitrd  = %s\n", initrd);
	    return -1;
	}

	hdb_set_enclave_dev_id(hobbes_master_db, enclave_id, pisces_id);

    }

    /* Boot the enclave with boot_env (if specified) */

    {
	int boot_cpu   = -1;
	int numa_zone  = -1;
	
	uintptr_t mem_size  = hobbes_get_block_size();
	uintptr_t base_addr = -1;

	pet_xml_t  boot_env_tree = pet_xml_get_subtree(xml, "boot_env");

	if (boot_env_tree != NULL) {
	    pet_xml_t   mem_tree = pet_xml_get_subtree(boot_env_tree, "memory");
	    char      * numa     = pet_xml_get_val(boot_env_tree,     "numa");
	    char      * cpu      = pet_xml_get_val(boot_env_tree,     "cpu");
	    char      * mem_blk  = pet_xml_get_val(boot_env_tree,     "memory");

	   numa_zone = smart_atoi   (numa_zone, numa);
	   boot_cpu  = smart_atoi   (boot_cpu,  cpu);
	   base_addr = smart_atou64 (base_addr, mem_blk);

	    if (mem_tree) {
		char * size_str = pet_xml_get_val(mem_tree, "size");

		mem_size = smart_atou64(0, size_str) * (1024 * 1024);

		if (mem_size == 0) {
		    mem_size = hobbes_get_block_size();
		}
	    }
	}


	/* Allocate anything not specified using default sizes (1 CPU, 1 memory block) */

	if (boot_cpu == -1) {
	    boot_cpu = hobbes_alloc_cpu(enclave_id, numa_zone);
	} else {
	    boot_cpu = hobbes_alloc_specific_cpu(enclave_id, boot_cpu);
	}

	if (boot_cpu == HOBBES_INVALID_CPU_ID) {
	    ERROR("Could not allocate Boot CPU for Pisces Enclave\n");
	    return -1;
	}


	if (base_addr == -1) {
	    base_addr = hobbes_alloc_mem(enclave_id, numa_zone, mem_size);

	    if (base_addr == 0) {
		ERROR("Could not allocate memory for Pisces Enclave\n");
		return -1;
	    }
	} else {
	    int ret = hobbes_alloc_mem_addr(enclave_id, base_addr, mem_size);
	    
	    if (ret == -1) {
		ERROR("Could not allocate specified memory (%p) for Pisces Enclave\n", (void *)base_addr);
		return -1;
	    }
	}

	printf("Launching Enclave\n");
	
	if (pisces_launch(pisces_id, numa_zone, boot_cpu, base_addr, mem_size, 1) != 0) {
	    ERROR("Could not launch pisces enclave (%d)\n", pisces_id);
	    ERROR("ERROR ERROR ERROR: We really need to implement this: pisces_free(pisces_id);\n");

	    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_CRASHED);

	    return -1;
	}


	printf("Launched\n");
    }

    /* Wait for 2 seconds for enclave userspace to initialize
     *  Check every 1/10th of a second
     */ 
    {
	int i = 0;

	for (i = 0; i < 20; i++) {
	    if (hdb_get_enclave_state(hobbes_master_db, enclave_id) == ENCLAVE_RUNNING) break;
	    usleep(100000);
	}

	if (i == 20) {
	    ERROR("ERROR: Enclave (%d)'s hobbes userspace did not initialize\n", enclave_id);
	    ERROR("Control may be established using legacy Pisces interface\n");
	    return -1;
	}
    }

    /* Dynamically add additional memory (if requested) */
    {
	pet_xml_t memory_tree = pet_xml_get_subtree(xml, "memory");

	while (memory_tree) {
	    uintptr_t size       =  0;
	    uint64_t  block_size =  0;
	    int       numa_node  = -1;
	    uintptr_t base_addr  = -1;

	    size       = smart_atou64(size,       pet_xml_get_val(memory_tree, "size")) * (1024 * 1024); /* Convert MB to bytes */
	    block_size = smart_atou64(block_size, pet_xml_get_val(memory_tree, "block_size")) * (1024 * 1024); /* Convert MB to bytes */
	    numa_node  = smart_atoi(numa_node,    pet_xml_get_val(memory_tree, "numa"));
	    base_addr  = smart_atou64(base_addr,  pet_xml_get_val(memory_tree, "base_addr"));

	    if (block_size == 0)
		block_size = hobbes_get_block_size();  

	    if ((size % block_size) != 0) {
		WARN("Size does not match system block size. Rounding up...\n");
		size += block_size - (size % block_size);
	    }

	    if (size > 0) {
		
		if (base_addr != -1) {

		    if (hobbes_alloc_mem_addr(enclave_id, base_addr, size) != 0) {
			WARN("Could not allocate memory region at (%p) to enclave (%d), continuing...\n", 
			     (void *)base_addr, enclave_id);
			goto mem_out;
		    }
		    
		    if (hobbes_assign_memory(enclave_id, base_addr, size, false, true) != 0) {
			ERROR("Error: Could not assign memory to enclave (%d)\n", enclave_id);
			hobbes_free_mem(base_addr, size);
			goto mem_out;
		    }

		} else {
		    uint32_t    num_regions = size / block_size;
		    uintptr_t * regions     = NULL;

		    int i = 0;

		    regions = calloc(sizeof(uintptr_t), num_regions);
		    
		    if (!regions) {
			ERROR("Could not allocate region array\n");
			goto mem_out;
		    }

		    if (hobbes_alloc_mem_regions(enclave_id, numa_node, num_regions, block_size, regions) != 0) {
			ERROR("Could not allocate %d memory regions for enclave (%d)\n", num_regions, enclave_id);
			goto mem_out;
		    }

		    for (i = 0; i < num_regions; i++) {
			if (hobbes_assign_memory(enclave_id, regions[i], block_size, false, true) != 0) {
			    ERROR("Error: Could not assign memory to enclave (%d)\n", enclave_id);
			    hobbes_free_mem(regions[i], block_size);
			    goto mem_out;
			}
		    }
		}
		
	    } else {
		WARN("Invalid size of memory region (%s). Ignoring...\n", 
		     pet_xml_tag_str(memory_tree));
	    }
	    
	mem_out:
	    memory_tree = pet_xml_get_next(memory_tree);
	
	}
    }
    
    /* Dynamically add additional CPUs (if requested) */
    {
	pet_xml_t cpus_tree = pet_xml_get_subtree(xml, "cpus");

	if (cpus_tree) {
	    pet_xml_t core_tree  = pet_xml_get_subtree(cpus_tree, "core");
	    pet_xml_t cores_tree = pet_xml_get_subtree(cpus_tree, "cores");
	    

	    while (core_tree) {
		int target = -1;

		target = smart_atoi(target, pet_xml_tag_str(core_tree));
		
		if (target >= 0) {
		    uint32_t cpu_id = hobbes_alloc_specific_cpu(enclave_id, target);

		    if (cpu_id == HOBBES_INVALID_CPU_ID) {
			WARN("Unable to allocate CPU (%d) for enclave (%d)\n", target, enclave_id);
			goto next;
		    }
		    
		    if (add_cpu_to_pisces(pisces_id, target) != 0) {

			WARN("Could not add CPU (%d) to enclave (%d), continuing...\n", 
			     target, enclave_id);

			hobbes_free_cpu(cpu_id);
		    }

		} else {
		    WARN("Invalid CPU index (%s) in enclave configuration. Ignoring...\n", 
			 pet_xml_tag_str(core_tree));
		}

	    next:
		core_tree = pet_xml_get_next(core_tree);
	    } 

	    while (cores_tree) {
		int numa_node  = -1;
		int core_count = -1;

		int i =  0;
		
		numa_node  = smart_atoi(numa_node,  pet_xml_get_val(cores_tree, "numa"));
		core_count = smart_atoi(core_count, pet_xml_tag_str(cores_tree));

		for (i = 0; i < core_count; i++) {
		    uint32_t cpu_id = hobbes_alloc_cpu(enclave_id, numa_node);
		    
		    if (cpu_id == HOBBES_INVALID_CPU_ID) {
			WARN("Only able to allocate %d CPUs for enclave (%d)\n", i, enclave_id);
			break;
		    }

		    if (add_cpu_to_pisces(pisces_id, cpu_id) != 0) {
			WARN("Could not add CPU (%d) to enclave (%d), continuing...\n", 
			     cpu_id, enclave_id);

			hobbes_free_cpu(cpu_id);
		    }

		}

		cores_tree = pet_xml_get_next(cores_tree);
	    }

	}
    }

    /* Dynamically add PCI devices (if requested) */
    {
	
	


    }


    return 0;
}


int
pisces_enclave_destroy(hobbes_id_t enclave_id)
{

    char * name   = hdb_get_enclave_name(hobbes_master_db, enclave_id);
    int    dev_id = hdb_get_enclave_dev_id(hobbes_master_db, enclave_id);

    if (pisces_teardown(dev_id, 1, 0) != 0) {
	ERROR("Could not teardown pisces enclave (%s)\n", name);
	return -1;
    }

    hobbes_free_enclave_mem(enclave_id);
    hobbes_free_enclave_cpus(enclave_id);

    if (hdb_delete_enclave(hobbes_master_db, enclave_id) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    return 0;
}




int
pisces_enclave_console(hobbes_id_t enclave_id)
{
    int    dev_id    = hdb_get_enclave_dev_id(hobbes_master_db, enclave_id);
    int    cons_fd   = pisces_get_cons_fd(dev_id);
    FILE * cons_file = NULL;

    char * line      = NULL;
    size_t line_size = 0;

    if (cons_fd < 0) {
	ERROR("Could not connect pisces enclave console (%s)\n", 
	      hdb_get_enclave_name(hobbes_master_db, enclave_id));
	return -1;
    }

    cons_file = fdopen(cons_fd, "r+");

    while (getline(&line, &line_size, cons_file) != -1) {
	printf("%s", line);
    }

    free(line);

    fclose(cons_file);

    return 0;
}
