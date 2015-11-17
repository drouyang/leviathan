/* Pisces VM control 
 *  (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <getopt.h>

#include <pet_log.h>


#include <hobbes.h>
#include <hobbes_enclave.h>
#include <hobbes_util.h>
#include <hobbes_system.h>
#include <hobbes_db.h>
#include <hobbes_notifier.h>

#include "vm.h"

/* Two options here:
 *  (1) We launch the VM into a currently running enclave, 
 *  OR 
 *  (2) We create a new dedicated enclave to host the VM
 */ 


extern hdb_db_t hobbes_master_db;


static void create_vm_usage() {
    printf("create_vm: Hobbes VM launch\n\n"				                \
           " Launches a VM as specified in command line options or in a job_file.\n\n"  \
           "Usage: create_vm <-f job_file | vm cfg args...> [options]\n"                \
           " Options: \n"						                \
           "\t-n, --name=<name>             : Name of VM                \n"		\
           "\t-t, --host=<enclave_name>     : Name of host enclave      \n"             \
	   "\t-h, --help                    : Display help dialog       \n"             \
           );

}



int 
create_vm_main(int argc, char ** argv)
{
    hobbes_id_t host_enclave_id = HOBBES_INVALID_ID;

    char      * cfg_file = NULL;
    char      * name     = NULL;
    char      * host     = NULL;

    /* Parse Options */
    {
        int  opt_index = 0;
        char c         = 0;

        opterr = 1;

        static struct option long_options[] = {
            {"name",     required_argument, 0,  'n'},
	    {"host",     required_argument, 0,  't'},
	    {"help",     no_argument,       0,  'h'},
	    {0, 0, 0, 0}
        };


        while ((c = getopt_long(argc, argv, "f:n:t:h", long_options, &opt_index)) != -1) {
            switch (c) {
                case 'f': 
		    cfg_file = optarg;
                    break;
		case 'n':
		    name = optarg;
		    break;
		case 't':
		    host = optarg;
		    break;
		case 'h':
		default:
		    create_vm_usage();
		    return -1;
	    }
	}
    }

    if (cfg_file == NULL) {
	create_vm_usage();
	return -1;
    }

    if (host) {
	host_enclave_id = hobbes_get_enclave_id(host);

	if (host_enclave_id == HOBBES_INVALID_ID) {
	    ERROR("Invalid Host enclave (%s)\n", host);
	    return -1;
	}
    }

  
    return hobbes_create_vm(cfg_file, name, host_enclave_id);

}


static int
__allocate_vm_memory(hobbes_id_t enclave_id, 
		     hobbes_id_t host_enclave_id,
		     pet_xml_t   xml)
{
    uintptr_t   block_size     = 0;
    char      * block_size_str = pet_xml_get_val(xml, "block_size");

    uintptr_t   mem_size       = 0;
    char      * mem_size_str   = pet_xml_get_val(xml, "size");

    pet_xml_t   reg_iter       = NULL;

    uint32_t    region_cnt     = 0;
    uintptr_t * regions        = NULL;
    int       * numa_arr       = NULL;

    char      * dflt_numa_str  = pet_xml_get_val(xml, "node");
    int         dflt_numa_node = smart_atoi(-1, dflt_numa_str);


    mem_size = smart_atou64(0, mem_size_str) * (1024 * 1024);

    if (mem_size == 0) {
	ERROR("Invalid Memory size in VM configuration file\n");
	return -1;
    }

    /* Check for block size */
    block_size = smart_atou64(hobbes_get_block_size(), block_size_str);

    
    /* 
     * For now lets make sure that the VM block size is never less than the system block size
     * This is not strictly necessary, but its not clear if we need to support the other case
     */
    if (block_size < hobbes_get_block_size()) {
	block_size = hobbes_get_block_size();
    }

    region_cnt = (mem_size / block_size) + ((mem_size % block_size) != 0);

    regions    = calloc(sizeof(uintptr_t), region_cnt);
    numa_arr   = calloc(sizeof(int),       region_cnt);
    
    if ((!regions) || (!numa_arr)) {
	ERROR("Could not allocate region arrays\n");
	goto err2;
    }


    /* Check for XML regions */
    reg_iter = pet_xml_get_subtree(xml, "region");

    if (!reg_iter) {
	uint32_t i   = 0;
	int      ret = 0;

	ret = hobbes_alloc_mem_regions(enclave_id, dflt_numa_node, region_cnt, block_size, regions);


	if (ret == -1) {
	    ERROR("Could not allocate VM memory\n");
	    goto err2;
	}

	for (i = 0; i < region_cnt; i++) {
	    numa_arr[i] = dflt_numa_node;
	}

    } else {
	int i = 0;
	pet_xml_t prev_reg = NULL;

	while (reg_iter) {
	    char * host_addr_str = pet_xml_get_val(reg_iter, "host_addr");
	    char * numa_node_str = pet_xml_get_val(reg_iter, "node");
	    char * size_str      = pet_xml_get_val(reg_iter, "size");
	    
	    uintptr_t size      = smart_atou64( 0, size_str);
	    uintptr_t host_addr = smart_atou64(-1, host_addr_str);

	    int       numa_node = smart_atoi(dflt_numa_node, numa_node_str);

	    if (size % block_size) {
		ERROR("Invalid Memory configuration (region size is not a multiple of the block size)\n");
		goto err2;
	    }

	    /* check for explicit mapping */
	    if (host_addr != (uintptr_t)-1) {
		uint64_t j = 0;
			
		if (hobbes_alloc_mem_addr(enclave_id, host_addr, size) == -1) {
		    ERROR("Could not allocate explicit memory block (host_addr=%p)\n", (void *)host_addr);
		    goto err1;
		}

		for (j = 0; j < (size / block_size); j++) {
		    
		    regions[i]  = host_addr;
		    numa_arr[i] = numa_node;

		    i         += 1;
		    host_addr += block_size;
		}
		
		
	    } else {
		uint64_t j = 0;

		for (j = 0; j < (size / block_size); j++) {
		    uintptr_t reg_addr = hobbes_alloc_mem(enclave_id, numa_node, block_size);

		    if (reg_addr == (uintptr_t)-1) {
			ERROR("Could not allocate region memory\n");
			goto err1;
		    }

		    regions[i]  = reg_addr;
		    numa_arr[i] = numa_node;

		    i++;
		}
	    }

	    prev_reg = reg_iter;
	    reg_iter = pet_xml_get_next(reg_iter);

	    pet_xml_del_subtree(prev_reg);
	}
    }


    /* Set XML configuration */
    {
	uint32_t i = 0;

	pet_xml_add_val(xml, "preallocated", "1");

	for (i = 0; i < region_cnt; i++) {
	    char * addr_str = NULL;
	    char * numa_str = NULL;
	    char * size_str = NULL;
	    
	    pet_xml_t reg_tree = pet_xml_add_subtree_tail(xml, "region");
	    
	    asprintf(&addr_str, "%p",  (void *)regions[i]);
	    asprintf(&numa_str, "%d",  numa_arr[i]);
	    asprintf(&size_str, "%lu", block_size); 
	    
	    pet_xml_add_val(reg_tree, "host_addr", addr_str);
	    pet_xml_add_val(reg_tree, "node",      numa_str);
	    pet_xml_add_val(reg_tree, "size",      size_str);
	    
	    free(addr_str);
	    free(numa_str);
	    free(size_str);
	}	
    }

    /* Assign memory to host enclave (if necessary) */
    if (hobbes_get_enclave_type(host_enclave_id) == PISCES_ENCLAVE) {
	uint32_t i = 0;
	
	for (i = 0; i < region_cnt; i++) {
	    if (hobbes_assign_memory(host_enclave_id, regions[i], block_size, true, false) != 0) {
		ERROR("Error: Could not assign VM memory to host enclave (%d)\n", host_enclave_id);
		goto err1;
	    }
	}
    }

    free(numa_arr);
    free(regions);

    return 0;

 err1: 
    {
	uint32_t i = 0;

	for (i = 0; i < region_cnt; i++) {
	    if (regions[i] != 0) {
		hobbes_free_mem(regions[i], block_size);
	    }
	}
    }

 err2:

    if (regions)  free(regions);
    if (numa_arr) free(numa_arr);

    return -1;
}


static int 
__create_vm(pet_xml_t   xml,
	    char      * name,
	    hobbes_id_t host_enclave_id)
{
    hobbes_id_t enclave_id   = INVALID_ENCLAVE;
    char      * xml_name     = NULL;
    char      * enclave_name = NULL;
    char      * target       = pet_xml_get_val(xml, "host_enclave");
    char      * err_str      = NULL;
    uint32_t    err_len      = 0;
    int         ret          = -1;

    if (host_enclave_id == HOBBES_INVALID_ID) {

	host_enclave_id = hobbes_get_enclave_id(target);
	
	if (host_enclave_id == HOBBES_INVALID_ID) {
	    ERROR("Could not find target Enclave (%s) for VM, launching on master enclave\n", target);
	    host_enclave_id = HOBBES_MASTER_ENCLAVE_ID;
	}
    }

    {
	xml_name = pet_xml_get_val(xml, "name");

	if (xml_name)
	    enclave_name = xml_name;
	else
	    enclave_name = name;

	/* Add VM to the Master DB */
	enclave_id = hdb_create_enclave(hobbes_master_db, 
					enclave_name, 
					-1, 
					VM_ENCLAVE, 
					host_enclave_id);

	if (enclave_id == -1) {
	    ERROR("Could not create enclave in database");
	    return -1;
	}

	if (xml_name == NULL) {
	    enclave_name = hobbes_get_enclave_name(enclave_id);
	    pet_xml_add_val(xml, "name", enclave_name);
	}	

    }

    {
	/* 
	 * Set the enclave_id in the VM XML file
	 */
	char * enclave_id_str = NULL;

	if (asprintf(&enclave_id_str, "%u", enclave_id) == -1) {
	    ERROR("Could not allocate enclave_id string\n");
	    goto err2;
	}

	pet_xml_add_val(xml, "enclave_id", enclave_id_str);
	free(enclave_id_str);
    }


    {
	/* Allocate memory */
	pet_xml_t mem_tree = pet_xml_get_subtree(xml, "memory");

	if (mem_tree == NULL) {
	    ERROR("Invalid VM Spec. Missing memory configuration block\n");
	    goto err1;
	}

	if (__allocate_vm_memory(enclave_id, host_enclave_id, mem_tree) == -1) {
	    ERROR("Could not allocate memory for VM\n");
	    goto err1;
	}


	
    }


    {
	/* 
	 * Temporary extension modification 
	 * This will move to config generation library when its done
	 */
	
	pet_xml_t ext_tree = pet_xml_get_subtree(xml, "extensions");
	pet_xml_t ext_iter = NULL;
	char    * id_str   = NULL;
	
	if (ext_tree == NULL) {
	    ext_tree = pet_xml_add_subtree(xml, "extensions");
	}

	ext_iter =  pet_xml_get_subtree(ext_tree, "extension");

	while (ext_iter != NULL) {
	    char * ext_name = pet_xml_get_val(ext_iter, "name");
	    
	    if (strncasecmp("HOBBES_ENV", ext_name, strlen("HOBBES_ENV")) == 0) {
		break;
	    }
	    
	    ext_iter = pet_xml_get_next(ext_iter);
	}
	
	if (ext_iter == NULL) {
	    ext_iter = pet_xml_add_subtree(ext_tree, "extension");
	    pet_xml_add_val(ext_iter, "name", "HOBBES_ENV");
	}
	
	asprintf(&id_str, "%u", enclave_id);
	pet_xml_add_val(ext_iter, "enclave_id", id_str);
	
	free(id_str);
    }


    {
	/* 
	 * Temporarily add the XPMEM device if it is not present. 
	 *  This will move to config generation library when its done
	 */
	pet_xml_t dev_tree = pet_xml_get_subtree(xml, "devices");
	pet_xml_t dev_iter = NULL;

	if (dev_tree == NULL) {
	    ERROR("Invalid VM config syntax. Missing devices section\n");
	    goto err2;
	}
	
	dev_iter = pet_xml_get_subtree(dev_tree, "device");
	
	while (dev_iter != NULL) {
	    char * dev_class = pet_xml_get_val(dev_iter, "class");
	    
	    if (strncasecmp("XPMEM", dev_class, strlen("XPMEM")) == 0) {
		break;
	    }
	    
	    dev_iter = pet_xml_get_next(dev_iter);
	}
	
	if (dev_iter == NULL) {
	    dev_iter = pet_xml_add_subtree_tail(dev_tree, "device");
	    pet_xml_add_val(dev_iter, "class", "XPMEM");
	    pet_xml_add_val(dev_iter, "id",    "XPMEM");
	    pet_xml_add_val(dev_iter, "bus",   "pci0");
	}
	
    }

    {

	/* Copy aux files over */

    }

    {
	hcq_handle_t hcq = hobbes_open_enclave_cmdq(host_enclave_id);
	hcq_cmd_t    cmd = HCQ_INVALID_CMD;
	char       * str = NULL;
	
	if (hcq == HCQ_INVALID_HANDLE) {
	    ERROR("Could not open command queue to enclave %d (%s)\n", 
		  host_enclave_id, hobbes_get_enclave_name(host_enclave_id));
	   goto err2;
	}

	str = pet_xml_get_str(xml);

	printf("vm config: (%s)\n", str);

	if (str == NULL) {
	    ERROR("Could not convert VM XML spec to string\n");	    
	    hobbes_close_enclave_cmdq(hcq);
	    goto err2;
	}

	printf("Strlen=%lu\n", strlen(str));

	cmd = hcq_cmd_issue(hcq, HOBBES_CMD_VM_LAUNCH, strlen(str) + 1, str);

	free(str);

	if (cmd == HCQ_INVALID_CMD) {
	    ERROR("Could not issue command to command queue\n");
	    hobbes_close_enclave_cmdq(hcq);
	    goto err2;
	}

	ret     = hcq_get_ret_code(hcq, cmd);	
	err_str = hcq_get_ret_data(hcq, cmd, &err_len);

	if (err_len > 0) {
	    printf("%s\n", err_str);
	}

	hcq_cmd_complete(hcq, cmd);
	hobbes_close_enclave_cmdq(hcq);
    }



    {
	hnotif_signal(HNOTIF_EVT_ENCLAVE);
    }



    return ret;


err2:
    /* Deallocate memory */
    hobbes_free_enclave_mem(enclave_id);

err1:
    /* Remove enclave from the database */
    hdb_delete_enclave(hobbes_master_db, enclave_id);
    
    return -1;
}

int
hobbes_destroy_vm(hobbes_id_t enclave_id)
{

    hobbes_id_t    host_enclave_id = HOBBES_INVALID_ID;
    enclave_type_t enclave_type    = INVALID_ENCLAVE;
    hcq_handle_t   hcq             = NULL;
    hcq_cmd_t      cmd             = HCQ_INVALID_CMD;

    char   * err_str =  NULL;
    uint32_t err_len =  0;
    int      ret     = -1;


   if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }

    enclave_type = hobbes_get_enclave_type(enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }
   
    if (enclave_type != VM_ENCLAVE) {
	ERROR("Enclave (%d) is not a VM enclave\n", enclave_id);
	return -1;
    }


    host_enclave_id = hobbes_get_enclave_parent(enclave_id);
    
    if (host_enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find parent enclave for enclave %d (%s)\n", 
	      enclave_id, hobbes_get_enclave_name(enclave_id));
	return -1;
    }

    hcq = hobbes_open_enclave_cmdq(host_enclave_id);
    
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open command queue for enclave %d (%s)\n", 
	      host_enclave_id, hobbes_get_enclave_name(host_enclave_id));
	return -1;
    }

    cmd = hcq_cmd_issue(hcq, HOBBES_CMD_VM_DESTROY, sizeof(hobbes_id_t), &enclave_id);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Could not issue command to command queue\n");
	hobbes_close_enclave_cmdq(hcq);
	return -1;
    }

    ret     = hcq_get_ret_code(hcq, cmd);	
    err_str = hcq_get_ret_data(hcq, cmd, &err_len);


    if (err_len > 0) {
	printf("%s\n", err_str);
    }

    hcq_cmd_complete(hcq, cmd);
    hobbes_close_enclave_cmdq(hcq);

    /* Free memory */
    hobbes_free_enclave_mem(enclave_id);
    
    if (ret == 0) {
	/* Remove enclave from the database */
	hdb_delete_enclave(hobbes_master_db, enclave_id);
    } else {	
	hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_ERROR);
    }


    {
	hnotif_signal(HNOTIF_EVT_ENCLAVE);
    }


	
    return ret;
}



int 
hobbes_create_vm(char        * cfg_file, 
		 char        * name,
		 hobbes_id_t   host_enclave_id)
{
    pet_xml_t   xml  = NULL;
    char      * type = NULL; 

    xml = pet_xml_open_file(cfg_file);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file);
	return -1;
    }

    type =  pet_xml_tag_name(xml);

    if (strncmp("vm", type, strlen("vm")) != 0) {
	ERROR("Invalid Enclave Type (%s)\n", type);
	return -1;
    }


    DEBUG("Creating VM Enclave\n");
    return __create_vm(xml, name, host_enclave_id);


}





int 
destroy_vm_main(int argc, char ** argv)
{
    hobbes_id_t    enclave_id   = HOBBES_INVALID_ID;

    if (argc < 1) {
	printf("Usage: hobbes destroy_vm <enclave name>\n");
	return -1;
    }
    
    enclave_id = hobbes_get_enclave_id(argv[1]);

    return hobbes_destroy_vm(enclave_id);
}
