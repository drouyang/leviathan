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
#include "pisces_ctrl.h"


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

    /* Add VM to the Master DB */
    {

	enclave_name = pet_xml_get_val(xml, "name");

	enclave_id = hdb_create_enclave(hobbes_master_db, 
					enclave_name, 
					-1, 
					PISCES_VM_ENCLAVE, 
					hobbes_get_my_enclave_id());

	if (enclave_id == -1) {
	    err_str = "Could not create enclave in database";
	    goto out1;
	}

	enclave_name = hdb_get_enclave_name(hobbes_master_db, enclave_id);

    }

    {
	/* 
	 * Temporary extension modification 
	 * This will move to config generation library when its done
	 */
	
	pet_xml_t ext_tree = pet_xml_get_subtree(xml, "extensions");
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
	    goto out1;
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

    if (type != PISCES_VM_ENCLAVE) {
	err_str = "Enclave is not a VM";
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
	goto out;
    }

    /* Free VM */
    
    ret = v3_free_vm(vm_id);

    if (ret == -1) {
	err_str = "Could not free VM";
	goto out;
    }


 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;

}


/** **************************************** **
 ** Pisces Commands                          **
 ** **************************************** **/



/**
 * This is copied (with minor tweaks) from the kitten pisces
 * control.
 *
 * Should it be preferrable to move this to v3vee.h. Making it
 * a library call?
 */
static int
__issue_vm_cmd(int       vm_id, 
	       u64       cmd, 
	       uintptr_t arg)
{
    char * dev_path = get_vm_dev_path(vm_id);
    int ret = 0;

    ret = pet_ioctl_path(dev_path, cmd, (void *)arg); 

    if (ret < 0) {
	printf("ERROR: Could not issue command (%llu) to guest (%d)\n", cmd, vm_id);
	return -1;
    }

    free(dev_path);

    return 0;
}

static int
__pisces_cons_connect(int      pisces_fd,
		      uint64_t cmd)
{
    struct cmd_vm_ctrl vm_cmd;
    uint64_t cons_ring_buf = 0;
    int ret;
    
    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));
    
    if (ret != sizeof(struct cmd_vm_ctrl)) {
        printf("Error reading console command\n");

        pisces_send_resp(pisces_fd, -1);
        return -1;
    }

    /* Signal Palacios to connect the console */
    if (__issue_vm_cmd(vm_cmd.vm_id, PISCES_VM_CONS_CONNECT,
		     (uintptr_t)&cons_ring_buf) == -1) {
        cons_ring_buf        = 0;
    }


    printf("Cons Ring Buf=%p\n", (void *)cons_ring_buf);
    pisces_send_resp(pisces_fd, cons_ring_buf);

    return 0;
}

static int
__pisces_cons_disconnect(int       pisces_fd,
			 uint64_t  cmd)
{
    struct cmd_vm_ctrl vm_cmd;
    int ret;

    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

    if (ret != sizeof(struct cmd_vm_ctrl)) {
        pisces_send_resp(pisces_fd, -1);
        return 0;
    }


    /* Send Disconnect Request to Palacios */
    if (__issue_vm_cmd(vm_cmd.vm_id, PISCES_VM_CONS_DISCONNECT, 
		     (uintptr_t)NULL) == -1) {
        pisces_send_resp(pisces_fd, -1);
        return 0;
    }

    pisces_send_resp(pisces_fd, 0);
    return 0;
}

static int
__pisces_cons_keycode(int      pisces_fd,
		      uint64_t cmd)
{
    struct cmd_vm_cons_keycode vm_cmd;
    int ret;

    ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_keycode));

    if (ret != sizeof(struct cmd_vm_cons_keycode)) {
        pisces_send_resp(pisces_fd, -1);
        return 0;
    }

    /* Send Keycode to Palacios */
    if (__issue_vm_cmd(vm_cmd.vm_id, V3_VM_KEYBOARD_EVENT, 
		     vm_cmd.scan_code) == -1) {
        pisces_send_resp(pisces_fd, -1);
        return 0;
    }

    pisces_send_resp(pisces_fd, 0);
    return 0;
}



int
palacios_init(void)
{

    // Register Hobbes commands
    if (hobbes_enabled) {
	hobbes_register_cmd(HOBBES_CMD_VM_LAUNCH,  __hobbes_launch_vm  );
	hobbes_register_cmd(HOBBES_CMD_VM_DESTROY, __hobbes_destroy_vm );
    }

    // Register Pisces commands
    register_pisces_cmd(PISCES_CMD_VM_CONS_CONNECT,    __pisces_cons_connect    );
    register_pisces_cmd(PISCES_CMD_VM_CONS_DISCONNECT, __pisces_cons_disconnect );
    register_pisces_cmd(PISCES_CMD_VM_CONS_KEYCODE,    __pisces_cons_keycode    );


    return 0;
}

bool 
palacios_is_available(void)
{
    return (v3_is_vmm_present() != 0);
}





/* Legacy Pisces Interfaces */
#if 0

	    case ENCLAVE_CMD_CREATE_VM: {
		struct pisces_user_file_info * file_info = NULL;
		struct cmd_create_vm vm_cmd;
		struct pmem_region rgn;

		id_t    my_aspace_id;
		vaddr_t file_addr;
		size_t  file_size =  0;
		int     path_len  =  0;
		int     vm_id     = -1;
		int     status    =  0;


		memset(&vm_cmd,    0, sizeof(struct cmd_create_vm));
		memset(&rgn,       0, sizeof(struct pmem_region));
			  
			    
		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_create_vm));

		if (ret != sizeof(struct cmd_create_vm)) {
		    pisces_send_resp(pisces_fd, -1);
		    printf("Error: CREATE_VM command could not be read\n");
		    break;
		}


		path_len = strlen((char *)vm_cmd.path.file_name) + 1;

		file_info = malloc(sizeof(struct pisces_user_file_info) + path_len);
		memset(file_info, 0, sizeof(struct pisces_user_file_info) + path_len);

		file_info->path_len = path_len;
		strncpy(file_info->path, (char *)vm_cmd.path.file_name, path_len - 1);
			    
		file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);

		
		status = aspace_get_myid(&my_aspace_id);
		if (status != 0) 
		    return status;

		if (pmem_alloc_umem(file_size, PAGE_SIZE, &rgn)) {
		    printf("Error: Could not allocate umem for guest image (size=%lu)\n", file_size);
		    break;
		}
		pmem_zero(&rgn);
				
		status =
		    aspace_map_region_anywhere(
					       my_aspace_id,
					       &file_addr,
					       round_up(file_size, PAGE_SIZE),
					       (VM_USER|VM_READ|VM_WRITE),
					       PAGE_SIZE,
					       "VM Image",
					       rgn.start
					       );


		file_info->user_addr = file_addr;
		
		ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);
				
				
		/* Issue VM Create command to Palacios */
		vm_id = v3_create_vm(vm_cmd.path.vm_name, (u8 *)file_info->user_addr, file_size);
				
		aspace_unmap_region(my_aspace_id, file_addr, round_up(file_size, PAGE_SIZE));
		pmem_free_umem(&rgn);
		

		if (vm_id < 0) {
		    printf("Error: Could not create VM (%s) at (%s) (err=%d)\n", 
			   vm_cmd.path.vm_name, vm_cmd.path.file_name, vm_id);
		    pisces_send_resp(pisces_fd, vm_id);
		    break;
		}

		printf("Created VM (%d)\n", vm_id);

		pisces_send_resp(pisces_fd, vm_id);
		break;
	    }
	    case ENCLAVE_CMD_FREE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_free_vm(vm_cmd.vm_id) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}


		pisces_send_resp(pisces_fd, 0);

		break;
	    }


   case ENCLAVE_CMD_FREE_V3_PCI: {
		struct cmd_add_pci_dev cmd;
		//		    struct v3_hw_pci_dev   v3_pci_spec;
		int ret = 0;

		memset(&cmd, 0, sizeof(struct cmd_add_pci_dev));

		printf("Removing V3 PCI Device\n");

		ret = read(pisces_fd, &cmd, sizeof(struct cmd_add_pci_dev));

		if (ret != sizeof(struct cmd_add_pci_dev)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		/*
		  memcpy(v3_pci_spec.name, cmd.spec.name, 128);
		  v3_pci_spec.bus  = cmd.spec.bus;
		  v3_pci_spec.dev  = cmd.spec.dev;
		  v3_pci_spec.func = cmd.spec.func;
		*/

		/* Issue Device Add operation to Palacios */
		/*
		  if (issue_v3_cmd(V3_REMOVE_PCI, (uintptr_t)&(v3_pci_spec)) == -1) {
		  printf("Error: Could not remove PCI device from Palacios\n");
		  pisces_send_resp(pisces_fd, -1);
		  break;
		  }
		*/
		pisces_send_resp(pisces_fd, 0);
		break;
	    }
	    case ENCLAVE_CMD_LAUNCH_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		/* Signal Palacios to Launch VM */
		if (v3_launch_vm(vm_cmd.vm_id) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}


		/*
		  if (xpmem_pisces_add_dom(palacios_fd, vm_cmd.vm_id)) {
		  printf("ERROR: Could not add connect to Palacios VM %d XPMEM channel\n", 
		  vm_cmd.vm_id);
		  }
		*/

		pisces_send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_STOP_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_stop_vm(vm_cmd.vm_id) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		pisces_send_resp(pisces_fd, 0);

		break;
	    }

	    case ENCLAVE_CMD_PAUSE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_pause_vm(vm_cmd.vm_id) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		pisces_send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_CONTINUE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_continue_vm(vm_cmd.vm_id) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}

		pisces_send_resp(pisces_fd, 0);

		break;
	    }



	    case ENCLAVE_CMD_VM_DBG: {
		struct cmd_vm_debug pisces_cmd;
			    
		ret = read(pisces_fd, &pisces_cmd, sizeof(struct cmd_vm_debug));
			    
		if (ret != sizeof(struct cmd_vm_debug)) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}
			    
			    
		if (v3_debug_vm(pisces_cmd.spec.vm_id, 
				pisces_cmd.spec.core, 
				pisces_cmd.spec.cmd) == -1) {
		    pisces_send_resp(pisces_fd, -1);
		    break;
		}
			    
		pisces_send_resp(pisces_fd, 0);
		break;
	    }




#endif
