#include <stdint.h>
#include <unistd.h>



#include <hobbes.h>
#include <hobbes_cmd_queue.h>
#include <hobbes_db.h>
#include <hobbes_util.h>

#include <v3vee.h>
#include <v3_ioctl.h>

#include <pet_ioctl.h>
#include <pet_log.h>
#include <pet_xml.h>

#include "palacios.h"
#include "hobbes_ctrl.h"


extern hdb_db_t hobbes_master_db;

static int
__hobbes_launch_vm(hcq_handle_t hcq,
		   hcq_cmd_t    cmd)
{
    hobbes_id_t enclave_id    = -1;

    pet_xml_t   xml           =  NULL;
    char      * xml_str       =  NULL;
    uint32_t    data_size     =  0;

    char      * enclave_name  =  NULL;
    int         vm_id         = -1;

    int         ret           = -1;
    char      * err_str       = NULL;


    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if ((xml_str == NULL) || (xml_str[data_size] != '\0')) {
	err_str = "Received invalid command string";
	goto out2;
    }

    xml = pet_xml_parse_str(xml_str);

    if (!xml) {
	err_str = "Invalid VM Spec";
	goto out2;
    }

    {
	/* Add VM to the Master DB */

	enclave_name = pet_xml_get_val(xml, "name");

	enclave_id = hdb_create_enclave(hobbes_master_db, 
					enclave_name, 
					-1, 
					LINUX_VM_ENCLAVE, 
					hobbes_get_my_enclave_id());

	if (enclave_id == -1) {
	    err_str = "Could not create enclave in database";
	    goto out1;
	}

	enclave_name = hdb_get_enclave_name(hobbes_master_db, enclave_id);

    }

    {
	/*
	 *  Temporary extension modification 
	 *  This will move to config generation library when its done
	 */
	pet_xml_t ext_tree = pet_xml_get_subtree(xml,      "extensions");
	pet_xml_t ext_iter = pet_xml_get_subtree(ext_tree, "extension");
	char * id_str = NULL;
	
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
	    return -1;
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


    /* Load VM Image */
    {
	u8 * img_data = NULL;
	u32  img_size = 0;

	img_data = v3_build_vm_image(xml, &img_size);

	if (img_data) {
	    vm_id = v3_create_vm(enclave_name, img_data, img_size);
	    
	    if (vm_id == -1) {
		err_str = "Could not create VM";
		ERROR("Could not create VM (%s)\n", enclave_name);
	    }
	} else {
	   err_str = "Could not build VM image from xml";
	}


	/* Cleanup if there was an error */
       	if ((img_data == NULL) || 
	    (vm_id    == -1)) {

	    hdb_delete_enclave(hobbes_master_db, enclave_id);
	    
	    goto out1;
	}


	hdb_set_enclave_dev_id(hobbes_master_db, enclave_id, vm_id);
    }

    
    /* Launch VM */
    {
	ret = v3_launch_vm(vm_id);

	if (ret != 0) {
	    err_str = "Could not launch VM enclave";
	    ERROR("ERROR ERROR ERROR: We really need to implement this: v3_free(vm_id);\n");

	    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_CRASHED);
	    
	    goto out1;
	}

    }

 out1:
    pet_xml_free(xml);
    
 out2:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;
}

static int
__hobbes_destroy_vm(hcq_handle_t hcq, 
		    hcq_cmd_t    cmd)
{
    enclave_type_t type       =  INVALID_ENCLAVE;
    hobbes_id_t  * enclave_id =  NULL;
    hobbes_id_t    parent_id  =  HOBBES_INVALID_ID;
    uint32_t       data_size  =  0;

    int            vm_id      = -1; 
    
    int            ret        = -1;
    char         * err_str    =  NULL;


    enclave_id = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (data_size != sizeof(hobbes_id_t)) {
	err_str = "Enclave ID is corrupt";
	goto out;
    } 
   
    /* Double check the enclave type is correct */

    type = hobbes_get_enclave_type(*enclave_id);

    if (type != LINUX_VM_ENCLAVE) {
	err_str = "Enclave is not a VM";
	goto out;
    }

    /* Double check that the VM is hosted by us */
    parent_id = hobbes_get_enclave_parent(*enclave_id);
    
    if (parent_id != hobbes_get_my_enclave_id()) {
	err_str = "VM is not hosted by this enclave\n";
	goto out;
    }

    /* Get local VM ID */

    vm_id = hobbes_get_enclave_dev_id(*enclave_id);
    
    if (vm_id == -1) {
	err_str = "Could not find VM instance";
	goto out;
    }

    /* Stop VM */

    ret = v3_stop_vm(vm_id);
    
    if (ret == -1) {
	err_str = "Could not stop VM";
	hobbes_set_enclave_state(*enclave_id, ENCLAVE_ERROR);
	goto out;
    }

    /* Free VM */
    
    ret = v3_free_vm(vm_id);

    if (ret == -1) {
	err_str = "Could not free VM";
	hobbes_set_enclave_state(*enclave_id, ENCLAVE_ERROR);
	goto out;
    }


    /* Remove enclave from the database */
    hdb_delete_enclave(hobbes_master_db, *enclave_id);


 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;

}


int
palacios_init(void)
{

    // Register Hobbes commands
    hobbes_register_cmd(HOBBES_CMD_VM_LAUNCH,  __hobbes_launch_vm  );
    hobbes_register_cmd(HOBBES_CMD_VM_DESTROY, __hobbes_destroy_vm );

     return 0;
}

bool 
palacios_is_available(void)
{
    return (v3_is_vmm_present() != 0);
}


