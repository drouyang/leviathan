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
add_cpus_to_pisces(int pisces_id,
		   int num_cpus, 
		   int numa_zone)
{
    int ret = -1;
    
    ret = pisces_add_cpus(pisces_id, num_cpus, numa_zone);

    return ret;
}

static int 
add_cpu_to_pisces(int pisces_id, 
		  int cpu_id)
{
    int ret = -1;

    ret = pisces_add_cpu(pisces_id, cpu_id);

    return ret;
}

static int
add_mem_node_to_pisces(int pisces_id, 
		       int numa_node)
{
    int ret = -1;
    
    ret = pisces_add_mem_node(pisces_id, numa_node);

    return ret;
}

static int 
add_mem_block_to_pisces(int pisces_id, 
			int block_id)
{
    int ret = -1;

    ret = pisces_add_mem_explicit(pisces_id, block_id);

    return ret;
}

static int 
add_mem_blocks_to_pisces(int pisces_id, 
			 int numa_node, 
			 int num_blocks, 
			 int contig)
{
    int ret = -1;

    if (contig == 0) {
	ret = pisces_add_mem(pisces_id, num_blocks, numa_node);
    } else {
	ERROR("Contiguous block allocations not yet supported\n");
	return -1;
    }


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
	int block_id   = -1;
	int num_blocks =  1;

	pet_xml_t  boot_env_tree = pet_xml_get_subtree(xml, "boot_env");

	if (boot_env_tree != NULL) {
	    pet_xml_t   mem_tree = pet_xml_get_subtree(boot_env_tree, "memory");
	    char      * numa     = pet_xml_get_val(boot_env_tree,     "numa");
	    char      * cpu      = pet_xml_get_val(boot_env_tree,     "cpu");
	    char      * mem_blk  = pet_xml_get_val(boot_env_tree,     "memory");

	   numa_zone = smart_atoi(numa_zone, numa);
	   boot_cpu  = smart_atoi(boot_cpu,  cpu);
	   block_id  = smart_atoi(block_id,  mem_blk);

	    if (mem_tree) {
		char * mem_size = pet_xml_get_val(mem_tree, "size");

		if (mem_size) {
		    int dflt_size_in_MB = pet_block_size() / (1024 * 1024);
		    int size_in_MB      = 0;

		    size_in_MB = smart_atoi(dflt_size_in_MB, mem_size);
		    
		    if (size_in_MB % dflt_size_in_MB) {
			WARN("Memory size is not a multiple of the HW block size [%lu].\n", pet_block_size());
			WARN("Memory size will be truncated.\n");
		    }

		    num_blocks = size_in_MB / dflt_size_in_MB;
		}
	    }
	}


	/* Allocate anything not specified using default sizes (1 CPU, 1 memory block) */
	if (boot_cpu == -1) {
	    // Allocate 1 boot cpu
	    
	}

	if (block_id == -1) {
	    // allocate num_blocks of memory
	    

	}


	
	if (pisces_launch(pisces_id, boot_cpu, numa_zone, block_id, num_blocks) != 0) {
	    ERROR("Could not launch pisces enclave (%d)\n", pisces_id);
	    ERROR("ERROR ERROR ERROR: We really need to implement this: pisces_free(pisces_id);\n");
	    

	    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_CRASHED);

	    return -1;
	}

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

	if (memory_tree) {
	    pet_xml_t block_tree  = pet_xml_get_subtree(memory_tree, "block");
	    pet_xml_t blocks_tree = pet_xml_get_subtree(memory_tree, "blocks");
	    pet_xml_t node_tree   = pet_xml_get_subtree(memory_tree, "node");

	    while (block_tree) {
		int block_id = -1;
		
		block_id = smart_atoi(block_id, pet_xml_tag_str(block_tree));

		if (block_id != -1) {
		    if (add_mem_block_to_pisces(pisces_id, block_id) != 0) {
			WARN("Could not add memory block <%d> to enclave (%d), continuing...\n", 
			     block_id, pisces_id);
		    }
		} else {
		    WARN("Invalid Block ID (%s) in memory configuration for enclave (%d), continuing...\n",
			 pet_xml_tag_str(block_tree), pisces_id);
		}
		    
			   

		block_tree = pet_xml_get_next(block_tree);
	    }
	    
	    while (node_tree) {
		int numa_node = -1;
		
		numa_node = smart_atoi(numa_node, pet_xml_tag_str(node_tree));

		if (numa_node != -1) {
		    if (add_mem_node_to_pisces(pisces_id, numa_node) != 0) {
			WARN("Could not add NUMA node <%d> to enclave (%d), continuing...\n", 
			     numa_node, pisces_id);
		    } 
		} else {
		    WARN("Invalid NUMA node (%s) in memory configuration for enclave (%d), continuing...\n",
			 pet_xml_tag_str(node_tree), pisces_id);
		}

		node_tree = pet_xml_get_next(node_tree);
	    }

	    
	    while (blocks_tree) {
		int numa_node  = -1;
		int num_blocks =  0;
		int contig     =  0;

		numa_node  = smart_atoi(numa_node,  pet_xml_get_val(blocks_tree, "numa"));
		contig     = smart_atoi(contig,     pet_xml_get_val(blocks_tree, "contig"));
		num_blocks = smart_atoi(num_blocks, pet_xml_tag_str(blocks_tree));		
		
		if (num_blocks > 0) {
		    if (add_mem_blocks_to_pisces(pisces_id, numa_node, num_blocks, contig) != 0) {
			WARN("Could not add memory blocks (%d) to enclave (%d), continuing...\n", 
			     num_blocks, pisces_id);
		    }
		} else {
		    WARN("Invalid number of blocks (%s) in enclave configuration. Ignoring...\n", 
			 pet_xml_tag_str(blocks_tree));
		}
		

		blocks_tree = pet_xml_get_next(blocks_tree);
	    }


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
		    if (add_cpu_to_pisces(pisces_id, target) != 0) {
			WARN("Could not add CPU (%d) to enclave (%d), continuing...\n", 
			     target, pisces_id);
		    }
		} else {
		    WARN("Invalid CPU index (%s) in enclave configuration. Ignoring...\n", 
			 pet_xml_tag_str(core_tree));
		}

		core_tree = pet_xml_get_next(core_tree);
	    } 

	    while (cores_tree) {
		int numa_node  = -1;
		int core_count = -1;

		numa_node  = smart_atoi(numa_node,  pet_xml_get_val(cores_tree, "numa"));
		core_count = smart_atoi(core_count, pet_xml_tag_str(cores_tree));

		if (core_count > 0) {
		    if (add_cpus_to_pisces(pisces_id, core_count, numa_node) != 0) {
			WARN("Could not add CPUs (%d) to enclave (%d), continuing...\n", 
			     core_count, pisces_id);
		    }
		} else {
		    WARN("Invalid CPU core count (%s) in enclave configuration. Ignoring...\n", 
			 pet_xml_tag_str(cores_tree));
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

    if (pisces_teardown(dev_id) != 0) {
	ERROR("Could not teardown pisces enclave (%s)\n", name);
	return -1;
    }

    if (hdb_delete_enclave(hobbes_master_db, enclave_id) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    return 0;
}

