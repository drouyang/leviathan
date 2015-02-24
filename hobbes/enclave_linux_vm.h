#include <v3vee.h>


#define PROC_PATH   "/proc/v3vee/"

#define MEM_HDR_STR       "BASE MEMORY REGIONS ([0-9]+)"
#define MEM_REGEX_STR     "[0-9]+: ([0-9A-Fa-f]{16}) - ([0-9A-Fa-f]{16})"

static int 
create_linux_vm(ezxml_t   xml, 
		char    * name)
{
    int vm_id      = -1;
    int enclave_id = -1;
    int ret        =  0;

    int * alloced_array  = NULL;
    int   alloced_blocks = 0;

    struct hobbes_enclave enclave;

    memset(&enclave, 0, sizeof(struct hobbes_enclave));


  
    /* Add enclave to the Master DB */
    {
	char * enclave_name = name; 

	if (enclave_name == NULL) {
	    enclave_name = get_val(xml, "name");
	}

	enclave_id = hdb_create_enclave(hobbes_master_db, 
					enclave_name, 
					vm_id, 
					LINUX_VM_ENCLAVE, 
					0);

	if (hdb_get_enclave_by_id(hobbes_master_db, enclave_id, &enclave) == 0) {
	    ERROR("Error creating enclave. could not find it...\n");
	    return -1;
	}
    }


    printf("Allocating memory for VM\n");
    /* Allocate memory for VM */
    {
	ezxml_t   mem_cfg  = get_subtree(xml,     "memory");
	ezxml_t   numa_cfg = get_subtree(mem_cfg, "region");

	if (numa_cfg != NULL) {
	    
	    alloced_array   = malloc(sizeof(int) * pet_num_numa_nodes());
	    memset(alloced_array, 0, sizeof(int) * pet_num_numa_nodes());

	    while (numa_cfg) {
		char * node_str = get_val(numa_cfg, "node");
		char * size_str = get_val(numa_cfg, "size");

		uint64_t mem_size  = 0;
		uint64_t num_blks  = 0;
		int      numa_zone = 0;

		// Get Memory size
		mem_size  = smart_atoi(0, size_str) * (1024 * 1024); 
		num_blks  = (mem_size + (pet_block_size() - 1)) / pet_block_size();
		numa_zone = smart_atoi(-1, node_str);

		// offline and add regions
		printf("Adding %d memory blocks to Palacios from NUMA node %d\n", num_blks, numa_zone);

		if (v3_add_mem(num_blks, numa_zone) == -1) {

		    ERROR("Could not add %d memory blocks to Palacios from NUMA zone %d\n", num_blks, numa_zone);
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

		numa_cfg = ezxml_next(numa_cfg);
	    }

	} else {
	    char     * mem_str  = get_val(mem_cfg, "size");
	    uint64_t   mem_size = 0;
	    uint64_t   num_blks = 0;
	    
	    // Get Memory size
	    mem_size = smart_atoi(0, mem_str) * (1024 * 1024); 
	    num_blks = (mem_size + (pet_block_size() - 1)) / pet_block_size();
	    
	    // offline and add regions
	    printf("Adding %d memory blocks to Palacios\n", num_blks);

	    if (v3_add_mem(num_blks, -1) == -1) {
		ERROR("Could not add %d memory blocks to Palacios\n", num_blks);
		hdb_delete_enclave(hobbes_master_db, enclave_id);
		return -1;
	    }
	    
	    alloced_blocks = num_blks;

	}
    }

    printf("Creating VM (%s)\n", enclave.name);


    /* Load VM Image */
    {
	u8 * img_data = NULL;
	u32  img_size = 0;
	int  ret      = 0;

	img_data = v3_build_vm_image(xml, &img_size);

	if (img_data) {
	    vm_id = v3_create_vm(enclave.name, img_data, img_size);
	    
	    if (vm_id == -1) {
		ERROR("Could not create VM (%s)\n", enclave.name);
	    }
	} else {
	    ERROR("Could not build VM image from xml\n");
	}


	/* Cleanup if there was an error */
       	if ((img_data == NULL) || 
	    (vm_id    == -1)) {

	    if (alloced_array) {
		int i = 0;

		for (i = 0 ; i < pet_num_numa_nodes(); i++) {			
		    if (alloced_array[i] > 0) {
			v3_remove_mem(alloced_array[i], i);
		    }
		}
	    }
	    
	    if (alloced_blocks > 0) {
		v3_remove_mem(alloced_blocks, -1);
	    }

	    hdb_delete_enclave(hobbes_master_db, enclave_id);
	    
	    return -1;
	}

    }


    /* Created successfully, record where to find it */
    {
	enclave.mgmt_dev_id = vm_id;
	hdb_update_enclave(hobbes_master_db, &enclave);
    }

    
    /* Launch VM */
    {
	ret = v3_launch_vm(vm_id);

	if (ret != 0) {
	    ERROR("Could not launch VM enclave (%d)\n", vm_id);
	    ERROR("ERROR ERROR ERROR: We really need to implement this: v3_free(vm_id);\n");
	    
	    enclave.state = ENCLAVE_CRASHED;
	    hdb_update_enclave(hobbes_master_db, &enclave);

	    return -1;
	}
    }
    

    {
	enclave.state = ENCLAVE_RUNNING;
	hdb_update_enclave(hobbes_master_db, &enclave);
    }


    return 0;
}


static int
destroy_linux_vm(struct hobbes_enclave * enclave)
{
    int * numa_block_list = NULL;

    /* grab memory in VM */

    /* Stop VM */

    /* Free VM */

    /* Release memory */

    /* Delete enclave from database */
    if (hdb_delete_enclave(hobbes_master_db, enclave->enclave_id) != 0) {
	ERROR("Could not delete enclave from database\n");
	return -1;
    }

    return 0;
}

