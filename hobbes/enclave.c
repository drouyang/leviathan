/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ezxml.h"

#include <pisces.h>
#include <pet_mem.h>


static ezxml_t 
open_xml_file(char * filename) 
{
    ezxml_t xml_input = ezxml_parse_file(filename);
    
    if (xml_input == NULL) {
	printf("Error: Could not open XML input file (%s)\n", filename);
	return NULL;
    } else if (strcmp("", ezxml_error(xml_input)) != 0) {
	printf("%s\n", ezxml_error(xml_input));
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
	printf("Invalid Cfg file: Duplicate value for %s (attr=%s, txt=%s)\n", 
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


    return -1;
}

static int 
add_cpu_to_pisces(int pisces_id, 
		  u64 cpu_id)
{



    return -1;
}



static int 
create_pisces_enclave(ezxml_t xml)
{
    int pisces_id = -1;

    /* Load Enclave OS files */
    if (0) {
	char * kern    = get_val(xml, "kernel");
	char * initrd  = get_val(xml, "init_task");
	char * cmdline = get_val(xml, "cmd_line");

	
	if ( (kern == NULL) || (initrd == NULL) ) {
	    fprintf(stderr, "Error: Must specify a kernel and init_task for a pisces_enclave\n");
	    return -1;
	} 
	
	pisces_id = pisces_load(kern, initrd, cmdline);

	if (pisces_id < 0) {
	    fprintf(stderr, "Error: Could not load Pisces Enclave\n");
	    fprintf(stderr, "\tkernel  = %s\n", kern);
	    fprintf(stderr, "\tinitrd  = %s\n", initrd);
	    fprintf(stderr, "\tcmdline = %s\n", cmdline);
	    return -1;
	}
    }


    /* Boot the enclave with boot_env (if specified) */

    if (0) {
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

	    if (numa)    { numa_zone = atoi(numa);    }
	    if (cpu)     { boot_cpu  = atoi(cpu);     }
	    if (mem_blk) { block_id  = atoi(mem_blk); }

	    if (mem_tree) {
		char * mem_size = get_val(mem_tree, "size");

		if (mem_size) {
		    int size_in_MB = atoi(mem_size);

		    num_blocks = (size_in_MB * 1024 * 1024)  / pet_block_size();
		}
	    }
	}

	
	if (pisces_launch(pisces_id, boot_cpu, numa_zone, block_id, num_blocks) != 0) {
	    fprintf(stderr, "Error: could not launch pisces enclave (%d)\n", pisces_id);
	    fprintf(stderr, "ERROR ERROR ERROR: We really need to implement this: pisces_free(pisces_id);\n");
	    return -1;
	}
    }

    /* Dynamically add additional memory (if requested) */
    {
	ezxml_t memory_tree = get_subtree(xml, "memory");

	if (memory_tree) {
	    ezxml_t block_tree  = get_subtree(memory_tree, "block");
	    ezxml_t blocks_tree = get_subtree(memory_tree, "blocks");
	    ezxml_t node_tree   = get_subtree(memory_tree, "node");

	    while (block_tree) {
		printf("%s\n", ezxml_txt(block_tree));
		block_tree = ezxml_next(block_tree);
	    }
	    
	    while (node_tree) {
		printf("%s\n", ezxml_txt(node_tree));
		node_tree = ezxml_next(node_tree);
	    }

	    
	    while (blocks_tree) {
		char * numa_node  = get_val(blocks_tree, "numa");
		char * contig     = get_val(blocks_tree, "contig");
		char * target     = get_val(blocks_tree, "target");
		char * num_blocks = ezxml_txt(blocks_tree);

		


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
		char * target = ezxml_txt(core_tree);

		core_tree = ezxml_next(core_tree);
	    } 

	    while (cores_tree) {
		char * numa  = get_val(core_tree, "numa");
		char * count = ezxml_txt(core_tree);

		cores_tree = ezxml_next(cores_tree);
	    }

	}
    }

    /* Dynamically add PCI devices (if requested) */
    {
	



    }



    return -1;
}

int 
create_enclave(char * cfg_file_name)
{
    ezxml_t   xml  = NULL;
    char    * type = NULL; 

    xml = open_xml_file(cfg_file_name);
    
    if (xml == NULL) {
	fprintf(stderr, "Error loading Enclave config file (%s)\n", cfg_file_name);
	return -1;
    }


    if (strncmp("enclave", ezxml_name(xml), strlen("enclave")) != 0) {
	fprintf(stderr, "Invalid XML Config: Not an enclave config\n");
	return -1;
    }

    type = get_val(xml, "type");
    
    if (type == NULL) {
	fprintf(stderr, "Enclave type not specified\n");
	return -1;
    }

    
    if (strncasecmp(type, "pisces", strlen("pisces")) == 0) {

	printf("Creating Pisces Enclave\n");
	return create_pisces_enclave(xml);

    } else if (strncasecmp(type, "vm", strlen("vm")) == 0) {

    } else {
	fprintf(stderr, "Error: Invalid Enclave Type (%s)\n", type);
	return -1;
    }



    return 0;

}
