














static int 
create_pisces_enclave(ezxml_t   xml, 
		      char    * name)
{
    int vm_id      = -1;
    int enclave_id = -1;

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

    /* Allocate memory for VM */
    {
	char * mem_str  = get_val(xml, "memory");
	int    mem_node = smart_atoi(-1, get_val(get_subtree(xml, "memory"), "node"));
	u64    mem_size = 0;
	u64    num_blks = 0;

	// Get Memory size
	mem_size = smart_atoi(0, mem_str) * (1024 * 1024); 

	num_blks = (mem_size + (pet_block_size() - 1)) / pet_block_size();

	// offline and add regions
	
	v3_add_mem(num_blks + 2, mem_node);

    }

    /* Load VM Image */
    {
	u8 * img_data = NULL;
	u32  img_size = 0;

	img_data = v3_build_vm_image(xml, &img_size);

	if (!img_data) {
	    hdb_delete_enclave(hobbes_master_db, enclave_id);
	    ERROR("Could not build VM image from xml\n");
	    return -1;
	}

	
	vm_id = v3_create_vm(name, img_data, img_size);

	if (vm_id == -1) {
	    hdb_delete_enclave(hobbes_master_db, enclave_id);
	    ERROR("Could not create VM (%s)\n", name);
	    return -1;
	}

    }

    /* Created successfully, record where to find it */
    {
	enclave.mgmt_dev_id = pisces_id;
	hdb_update_enclave(hobbes_master_db, &enclave);
    }

    

    return 0;
}
