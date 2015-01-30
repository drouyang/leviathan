/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hobbes_types.h"
#include "client.h"

#include <pisces.h>
#include <pet_mem.h>
#include <pet_log.h>
#include <ezxml.h>


static int
smart_atoi(int dflt, char * str) 
{
    char * end = NULL;
    int    tmp = 0;
    
    if ((str == NULL) || (*str == '\0')) {
	/*  String was either NULL or empty */
	return dflt;
    }

    tmp = strtol(str, &end, 10);

    if (*end) {
	/* String contained non-numerics */
	return dflt;
    }
    
    return tmp;
}

static ezxml_t 
open_xml_file(char * filename) 
{
    ezxml_t xml_input = ezxml_parse_file(filename);
    
    if (xml_input == NULL) {
	ERROR("Could not open XML input file (%s)\n", filename);
	return NULL;
    } else if (strcmp("", ezxml_error(xml_input)) != 0) {
	ERROR("%s\n", ezxml_error(xml_input));
	return NULL;
    }

    return xml_input;
}



static char * 
get_val(ezxml_t   cfg,
	char    * tag) 
{
    char   * attrib = (char *)ezxml_attr(cfg, tag);
    ezxml_t  txt    = ezxml_child(cfg, tag);
    char   * val    = NULL;

    if ((txt != NULL) && (attrib != NULL)) {
	ERROR("Invalid Cfg file: Duplicate value for %s (attr=%s, txt=%s)\n", 
	       tag, attrib, ezxml_txt(txt));
	return NULL;
    }

    val = (attrib == NULL) ? ezxml_txt(txt) : attrib;

    /* An non-present value actually == "". So we check if the 1st char is '/0' and return NULL */
    if (!*val) return NULL;

    return val;
}


static ezxml_t 
get_subtree(ezxml_t   tree,
	    char    * tag) 
{
    return ezxml_child(tree, tag);
}


static ezxml_t
get_next_branch(ezxml_t tree) 
{
    return ezxml_next(tree);
}


/*

static int
read_file(int             fd, 
	  int             size, 
	  unsigned char * buf) 
{
    int left_to_read = size;
    int have_read    = 0;

    while (left_to_read != 0) {
	int bytes_read = read(fd, buf + have_read, left_to_read);

	if (bytes_read <= 0) {
	    break;
	}

	have_read    += bytes_read;
	left_to_read -= bytes_read;
    }

    if (left_to_read != 0) {
	printf("Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}

*/


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




	   


static int 
create_pisces_enclave(ezxml_t   xml, 
		      char    * name)
{
    int pisces_id = -1;

    /* Load Enclave OS files */
    {
	char * kern    = get_val(xml, "kernel");
	char * initrd  = get_val(xml, "init_task");
	char * cmdline = get_val(xml, "cmd_line");

	
	if ( (kern == NULL) || (initrd == NULL) ) {
	    ERROR("Must specify a kernel and init_task for a pisces_enclave\n");
	    return -1;
	} 
	
	pisces_id = pisces_load(kern, initrd, cmdline);

	if (pisces_id < 0) {
	    ERROR("Could not load Pisces Enclave\n");
	    ERROR("\tkernel  = %s\n", kern);
	    ERROR("\tinitrd  = %s\n", initrd);
	    ERROR("\tcmdline = %s\n", cmdline);
	    return -1;
	}
    }


    /* Boot the enclave with boot_env (if specified) */

    {
	int boot_cpu   = -1;
	int numa_zone  = -1;
	int block_id   = -1;
	int num_blocks =  1;

	ezxml_t  boot_env_tree = get_subtree(xml, "boot_env");

	if (boot_env_tree != NULL) {
	    ezxml_t   mem_tree = get_subtree(boot_env_tree, "memory");
	    char    * numa     = get_val(boot_env_tree, "numa");
	    char    * cpu      = get_val(boot_env_tree, "cpu");
	    char    * mem_blk  = get_val(boot_env_tree, "memory");

	   numa_zone = smart_atoi(numa_zone, numa);
	   boot_cpu  = smart_atoi(boot_cpu,  cpu);
	   block_id  = smart_atoi(block_id,  mem_blk);

	    if (mem_tree) {
		char * mem_size = get_val(mem_tree, "size");

		if (mem_size) {
		    int dflt_size_in_MB = pet_block_size() / (1024 * 1024);
		    int size_in_MB      = 0;

		    size_in_MB = smart_atoi(dflt_size_in_MB, mem_size);
		    
		    if (size_in_MB % dflt_size_in_MB) {
			WARN("Memory size is not a multiple of the HW block size [%d].\n", pet_block_size());
			WARN("Memory size will be truncated.\n");
		    }

		    num_blocks = size_in_MB / dflt_size_in_MB;
		}
	    }
	}

	
	if (pisces_launch(pisces_id, boot_cpu, numa_zone, block_id, num_blocks) != 0) {
	    ERROR("Could not launch pisces enclave (%d)\n", pisces_id);
	    ERROR("ERROR ERROR ERROR: We really need to implement this: pisces_free(pisces_id);\n");
	    return -1;
	}
    }

    /* Add enclave to the Master DB */
    {
	char * enclave_name = name; 

	if (enclave_name == NULL) {
	    enclave_name = get_val(xml, "name");
	}

	hdb_insert_enclave(hobbes_master_db, enclave_name, pisces_id, PISCES_ENCLAVE, 0);

    }
    

    /* Dynamically add additional memory (if requested) */
    {
	ezxml_t memory_tree = get_subtree(xml, "memory");

	if (memory_tree) {
	    ezxml_t block_tree  = get_subtree(memory_tree, "block");
	    ezxml_t blocks_tree = get_subtree(memory_tree, "blocks");
	    ezxml_t node_tree   = get_subtree(memory_tree, "node");

	    while (block_tree) {
		int block_id = -1;
		
		block_id = smart_atoi(block_id, ezxml_txt(block_tree));

		if (block_id != -1) {
		    if (add_mem_block_to_pisces(pisces_id, block_id) != 0) {
			WARN("Could not add memory block <%d> to enclave (%d), continuing...\n", 
			     block_id, pisces_id);
		    }
		} else {
		    WARN("Invalid Block ID (%s) in memory configuration for enclave (%d), continuing...\n",
			 ezxml_txt(block_tree), pisces_id);
		}
		    
			   

		block_tree = ezxml_next(block_tree);
	    }
	    
	    while (node_tree) {
		int numa_node = -1;
		
		numa_node = smart_atoi(numa_node, ezxml_txt(node_tree));

		if (numa_node != -1) {
		    if (add_mem_node_to_pisces(pisces_id, numa_node) != 0) {
			WARN("Could not add NUMA node <%d> to enclave (%d), continuing...\n", 
			     numa_node, pisces_id);
		    } 
		} else {
		    WARN("Invalid NUMA node (%s) in memory configuration for enclave (%d), continuing...\n",
			 ezxml_txt(node_tree), pisces_id);
		}

		node_tree = ezxml_next(node_tree);
	    }

	    
	    while (blocks_tree) {
		int numa_node  = -1;
		int num_blocks =  0;
		int contig     =  0;

		numa_node  = smart_atoi(numa_node,  get_val(blocks_tree, "numa"));
		contig     = smart_atoi(contig,     get_val(blocks_tree, "contig"));
		num_blocks = smart_atoi(num_blocks, ezxml_txt(blocks_tree));		
		
		if (num_blocks > 0) {
		    if (add_mem_blocks_to_pisces(pisces_id, numa_node, num_blocks, contig) != 0) {
			WARN("Could not add memory blocks (%d) to enclave (%d), continuing...\n", 
			     num_blocks, pisces_id);
		    }
		} else {
		    WARN("Invalid number of blocks (%s) in enclave configuration. Ignoring...\n", 
			 ezxml_txt(blocks_tree));
		}
		

		blocks_tree = ezxml_next(blocks_tree);
	    }


	}
    }

    /* Dynamically add additional CPUs (if requested) */
    {
	ezxml_t cpus_tree = get_subtree(xml, "cpus");

	if (cpus_tree) {
	    ezxml_t core_tree  = get_subtree(cpus_tree, "core");
	    ezxml_t cores_tree = get_subtree(cpus_tree, "cores");
	    

	    while (core_tree) {
		int target = -1;

		target = smart_atoi(target, ezxml_txt(core_tree));
		
		if (target >= 0) {
		    if (add_cpu_to_pisces(pisces_id, target) != 0) {
			WARN("Could not add CPU (%d) to enclave (%d), continuing...\n", 
			     target, pisces_id);
		    }
		} else {
		    WARN("Invalid CPU index (%s) in enclave configuration. Ignoring...\n", 
			 ezxml_txt(core_tree));
		}

		core_tree = ezxml_next(core_tree);
	    } 

	    while (cores_tree) {
		int numa_node  = -1;
		int core_count = -1;

		numa_node  = smart_atoi(numa_node,  get_val(cores_tree, "numa"));
		core_count = smart_atoi(core_count, ezxml_txt(cores_tree));

		if (core_count > 0) {
		    if (add_cpus_to_pisces(pisces_id, core_count, numa_node) != 0) {
			WARN("Could not add CPUs (%d) to enclave (%d), continuing...\n", 
			     core_count, pisces_id);
		    }
		} else {
		    WARN("Invalid CPU core count (%s) in enclave configuration. Ignoring...\n", 
			 ezxml_txt(cores_tree));
		}

		cores_tree = ezxml_next(cores_tree);
	    }

	}
    }

    /* Dynamically add PCI devices (if requested) */
    {
	



    }



    return -1;
}


static int
destroy_pisces_enclave(struct hobbes_enclave * enclave)
{
    if (pisces_teardown(enclave->mgmt_dev_id) != 0) {
	ERROR("Could not teardown pisces enclave (%s)\n", enclave->name);
	return -1;
    }

    if (hdb_delete_enclave(hobbes_master_db, enclave->enclave_id) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    return 0;
}



int 
create_enclave(char * cfg_file_name, 
	       char * name)
{
    ezxml_t   xml  = NULL;
    char    * type = NULL; 

    xml = open_xml_file(cfg_file_name);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file_name);
	return -1;
    }


    if (strncmp("enclave", ezxml_name(xml), strlen("enclave")) != 0) {
	ERROR("Invalid XML Config: Not an enclave config\n");
	return -1;
    }

    type = get_val(xml, "type");
    
    if (type == NULL) {
	ERROR("Enclave type not specified\n");
	return -1;
    }

    
    if (strncasecmp(type, "pisces", strlen("pisces")) == 0) {

	DEBUG("Creating Pisces Enclave\n");
	return create_pisces_enclave(xml, name);

    } else if (strncasecmp(type, "vm", strlen("vm")) == 0) {

    } else {
	ERROR("Invalid Enclave Type (%s)\n", type);
	return -1;
    }



    return 0;

}



int 
destroy_enclave(char * enclave_name)
{
    struct hobbes_enclave enclave;

    if (hdb_get_enclave_by_name(hobbes_master_db, enclave_name, &enclave) != 1) {
	ERROR("Could not find enclave (%s)\n", enclave_name);
	return -1;
    }
    
    if (enclave.type == PISCES_ENCLAVE) {
	return destroy_pisces_enclave(&enclave);
    } else {
	ERROR("Invalid Enclave Type (%d)\n", enclave.type);
	return -1;
    }

    return 0;
    
}






const char * 
enclave_type_to_str(enclave_type_t type) 
{
    switch (type) {
	case INVALID_ENCLAVE:   return "INVALID_ENCLAVE";
	case MASTER_ENCLAVE:    return "MASTER_ENCLAVE";
	case PISCES_ENCLAVE:    return "PISCES_ENCLAVE";
	case PISCES_VM_ENCLAVE: return "PISCES_VM_ENCLAVE";
	case LINUX_VM_ENCLAVE:  return "LINUX_VM_ENCLAVE";

	default : return NULL;
    }

    return NULL;
}
