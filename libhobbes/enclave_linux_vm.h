#include <v3vee.h>

#include <pet_numa.h>
#include <pet_mem.h>

#define PROC_PATH   "/proc/v3vee/"

#define MEM_HDR_STR       "BASE MEMORY REGIONS ([0-9]+)"
#define MEM_REGEX_STR     "[0-9]+: ([0-9A-Fa-f]{16}) - ([0-9A-Fa-f]{16})"



static int
destroy_linux_vm(hobbes_id_t enclave_id)
{
    int  * numa_block_list = NULL;
    int    dev_id          = -1;
    char * enclave_name    = NULL;

    enclave_name = hdb_get_enclave_name(hobbes_master_db, enclave_id);
    dev_id       = hdb_get_enclave_dev_id(hobbes_master_db, enclave_id);

    numa_block_list = calloc(pet_num_numa_nodes(), sizeof(int));

    {

	char * proc_filename   = NULL;
	FILE * proc_file       = NULL;
	char * line            = NULL;
	size_t line_size       = 0;

	int num_blks = 0;
	int matched  = 0;
	int i        = 0;

	/* grab memory in VM */
	if (asprintf(&proc_filename, PROC_PATH "v3-vm%d/mem", dev_id) == -1) {
	    ERROR("asprintf failed\n");
	    goto err;
	}

	proc_file = fopen(proc_filename, "r");

	free(proc_filename);
    
	if (proc_file == NULL) {
	    ERROR("Could not open proc file for enclave %s (VM %d)\n", enclave_name, dev_id);
	    goto err;
	}

	if (getline(&line, &line_size, proc_file) == -1) {
	    ERROR("Could not read VM proc file for enclave %s\n", enclave_name);
	    goto err;
	}
	
	matched = sscanf(line, "BASE MEMORY REGIONS (%d)", &num_blks);
	
	free(line);

	if (matched != 1) {
	    ERROR("Could not parse VM information proc file (memory header)\n");
	    goto err;
	}
	

	for (i = 0; i < num_blks; i++) {
	    uint64_t start_addr = 0;
	    uint64_t end_addr   = 0;
	    uint32_t blk_size   = 0;
	    int      numa_zone  = 0;
	    
	    line = NULL;

	    if (getline(&line, &line_size, proc_file) == -1) {
		ERROR("Could not read VM proc file for enclave %s\n", enclave_name);
		goto err;
	    }

	    matched = sscanf(line, "       0x%lx - 0x%lx  (size=%uMB) [NUMA ZONE=%d]", 
			     &start_addr, &end_addr, &blk_size, &numa_zone);
	    free(line);
	
	    if (matched != 4) {
		ERROR("Parsing error for VM memory blocks\n");
		goto err;
	    }
	    
	    numa_block_list[numa_zone]++;
	}
    }



    /* Stop VM */

    {
	int ret = v3_stop_vm(dev_id);

	if (ret == -1) {
	    ERROR("Could not stop Linux VM enclave %s\n", enclave_name);
	    goto corrupt_err;
	}
    }

    /* Free VM */

    {
	int ret = v3_free_vm(dev_id);

	if (ret == -1) {
	    ERROR("Could not free Linux VM enclave %s\n", enclave_name);
	    goto corrupt_err;
	}
    }


    /* Release memory */
    {
	int i = 0;

	for (i = 0; i < pet_num_numa_nodes(); i++) {
	    v3_remove_mem(numa_block_list[i], i);
	}
    }


    /* Delete enclave from database */
    if (hdb_delete_enclave(hobbes_master_db, enclave_id) != 0) {
	ERROR("Could not delete enclave from database\n");
	goto corrupt_err;
    }

    free(numa_block_list);

    return 0;

 corrupt_err:
    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_ERROR);
 err:
    free(numa_block_list);
    return -1;
}



static int 
create_linux_vm(pet_xml_t   xml, 
		char      * name)
{
    hobbes_id_t enclave_id = -1;

    char * enclave_name    =  name; 
    int    vm_id           = -1;
    int    ret             =  0;

    int  * alloced_array   =  NULL;
    int    alloced_blocks  =  0;

 
  
    /* Add enclave to the Master DB */
    {

	if (enclave_name == NULL) {
	    enclave_name = pet_xml_get_val(xml, "name");
	}

	enclave_id = hdb_create_enclave(hobbes_master_db, 
					enclave_name, 
					vm_id, 
					LINUX_VM_ENCLAVE, 
					0);

	if (enclave_id == -1) {
            ERROR("Could not create enclave in database\n");
            return -1;
	}

	enclave_name = hdb_get_enclave_name(hobbes_master_db, enclave_id);

    }


    printf("Allocating memory for VM\n");
    /* Allocate memory for VM */
    {
	pet_xml_t   mem_cfg  = pet_xml_get_subtree(xml,     "memory");
	pet_xml_t   numa_cfg = pet_xml_get_subtree(mem_cfg, "region");

	if (numa_cfg != NULL) {
	    
	    alloced_array   = malloc(sizeof(int) * pet_num_numa_nodes());
	    memset(alloced_array, 0, sizeof(int) * pet_num_numa_nodes());

	    while (numa_cfg) {
		char * node_str = pet_xml_get_val(numa_cfg, "node");
		char * size_str = pet_xml_get_val(numa_cfg, "size");

		uint64_t mem_size  = 0;
		uint64_t num_blks  = 0;
		int      numa_zone = 0;

		// Get Memory size
		mem_size  = smart_atoi(0, size_str) * (1024 * 1024); 
		num_blks  = (mem_size + (pet_block_size() - 1)) / pet_block_size();
		numa_zone = smart_atoi(-1, node_str);

		// offline and add regions
		printf("Adding %lu memory blocks to Palacios from NUMA node %d\n", num_blks, numa_zone);

		if (v3_add_mem(num_blks, numa_zone) == -1) {

		    ERROR("Could not add %lu memory blocks to Palacios from NUMA zone %d\n", num_blks, numa_zone);

		    hdb_delete_enclave(hobbes_master_db, enclave_id);

		    /* Free previous memory... */
		    {
			int i = 0;
			
			for (i = 0 ; i < pet_num_numa_nodes(); i++) {			
			    if (alloced_array[i] > 0) {
				v3_remove_mem(alloced_array[i], i);
			    }
			}
		    }

		    return -1;
		}

		alloced_array[numa_zone] = num_blks;

		numa_cfg = pet_xml_get_next(numa_cfg);
	    }

	} else {
	    char     * mem_str  = pet_xml_get_val(mem_cfg, "size");
	    uint64_t   mem_size = 0;
	    uint64_t   num_blks = 0;
	    
	    // Get Memory size
	    mem_size = smart_atoi(0, mem_str) * (1024 * 1024); 
	    num_blks = (mem_size + (pet_block_size() - 1)) / pet_block_size();
	    
	    // offline and add regions
	    printf("Adding %lu memory blocks to Palacios\n", num_blks);

	    if (v3_add_mem(num_blks, -1) == -1) {
		ERROR("Could not add %lu memory blocks to Palacios\n", num_blks);

		hdb_delete_enclave(hobbes_master_db, enclave_id);

		return -1;
	    }
	    
	    alloced_blocks = num_blks;

	}
    }


    printf("Creating VM (%s)\n", enclave_name);

    printf("Alloced blocks=%d\n", alloced_blocks);


    /* Load VM Image */
    {
	u8 * img_data = NULL;
	u32  img_size = 0;

	img_data = v3_build_vm_image(xml, &img_size);

	if (img_data) {
	    vm_id = v3_create_vm(enclave_name, img_data, img_size);
	    
	    if (vm_id == -1) {
		ERROR("Could not create VM (%s)\n", enclave_name);
	    }
	} else {
	    ERROR("Could not build VM image from xml\n");
	}


	/* Cleanup if there was an error */
       	if ((img_data == NULL) || 
	    (vm_id    == -1)) {

	    printf("Creation error\n");

	    if (alloced_array) {
		int i = 0;

		for (i = 0 ; i < pet_num_numa_nodes(); i++) {			
		    if (alloced_array[i] > 0) {
			v3_remove_mem(alloced_array[i], i);
		    }
		}
	    }
	    
	    if (alloced_blocks > 0) {
		printf("Removing %d allocated blocks\n", alloced_blocks);
		v3_remove_mem(alloced_blocks, -1);
	    }

	    hdb_delete_enclave(hobbes_master_db, enclave_id);
	    
	    return -1;
	}


	hdb_set_enclave_dev_id(hobbes_master_db, enclave_id, vm_id);
    }

    
    /* Launch VM */
    {
	ret = v3_launch_vm(vm_id);

	if (ret != 0) {
	    ERROR("Could not launch VM enclave (%d)\n", vm_id);
	    ERROR("ERROR ERROR ERROR: We really need to implement this: v3_free(vm_id);\n");

	    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_CRASHED);
	    
	    return -1;
	}

	hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_RUNNING);
    }
    

    return 0;
}
