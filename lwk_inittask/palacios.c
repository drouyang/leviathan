#include <v3vee.h>
#include <pet_log.h>

bool v3_enabled = false;




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
		    send_resp(pisces_fd, -1);
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
		    send_resp(pisces_fd, vm_id);
		    break;
		}

		printf("Created VM (%d)\n", vm_id);

		send_resp(pisces_fd, vm_id);
		break;
	    }
	    case ENCLAVE_CMD_FREE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_free_vm(vm_cmd.vm_id) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}


		send_resp(pisces_fd, 0);

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
		    send_resp(pisces_fd, -1);
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
		  send_resp(pisces_fd, -1);
		  break;
		  }
		*/
		send_resp(pisces_fd, 0);
		break;
	    }
	    case ENCLAVE_CMD_LAUNCH_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		/* Signal Palacios to Launch VM */
		if (v3_launch_vm(vm_cmd.vm_id) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}


		/*
		  if (xpmem_pisces_add_dom(palacios_fd, vm_cmd.vm_id)) {
		  printf("ERROR: Could not add connect to Palacios VM %d XPMEM channel\n", 
		  vm_cmd.vm_id);
		  }
		*/

		send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_STOP_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_stop_vm(vm_cmd.vm_id) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		send_resp(pisces_fd, 0);

		break;
	    }

	    case ENCLAVE_CMD_PAUSE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_pause_vm(vm_cmd.vm_id) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_CONTINUE_VM: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		if (v3_continue_vm(vm_cmd.vm_id) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		send_resp(pisces_fd, 0);

		break;
	    }
	    case ENCLAVE_CMD_VM_CONS_CONNECT: {
		struct cmd_vm_ctrl vm_cmd;
		u64 cons_ring_buf = 0;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    printf("Error reading console command\n");

		    send_resp(pisces_fd, -1);
		    break;
		}

		/* Signal Palacios to connect the console */
		/*
		  if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_CONNECT, (uintptr_t)&cons_ring_buf) == -1) {
		  cons_ring_buf        = 0;
		  }
		*/	

		printf("Cons Ring Buf=%p\n", (void *)cons_ring_buf);
		send_resp(pisces_fd, cons_ring_buf);

		break;
	    }

	    case ENCLAVE_CMD_VM_CONS_DISCONNECT: {
		struct cmd_vm_ctrl vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_ctrl));

		if (ret != sizeof(struct cmd_vm_ctrl)) {
		    send_resp(pisces_fd, -1);
		    break;
		}


		/* Send Disconnect Request to Palacios */
		/*
		  if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_CONSOLE_DISCONNECT, (uintptr_t)NULL) == -1) {
		  send_resp(pisces_fd, -1);
		  break;
		  }
		*/

		send_resp(pisces_fd, 0);
		break;
	    }

	    case ENCLAVE_CMD_VM_CONS_KEYCODE: {
		struct cmd_vm_cons_keycode vm_cmd;

		ret = read(pisces_fd, &vm_cmd, sizeof(struct cmd_vm_cons_keycode));

		if (ret != sizeof(struct cmd_vm_cons_keycode)) {
		    send_resp(pisces_fd, -1);
		    break;
		}

		/* Send Keycode to Palacios */
		/*
		  if (issue_vm_cmd(vm_cmd.vm_id, V3_VM_KEYBOARD_EVENT, vm_cmd.scan_code) == -1) {
		  send_resp(pisces_fd, -1);
		  break;
		  }
		*/
		send_resp(pisces_fd, 0);
		break;
	    }

	    case ENCLAVE_CMD_VM_DBG: {
		struct cmd_vm_debug pisces_cmd;
			    
		ret = read(pisces_fd, &pisces_cmd, sizeof(struct cmd_vm_debug));
			    
		if (ret != sizeof(struct cmd_vm_debug)) {
		    send_resp(pisces_fd, -1);
		    break;
		}
			    
			    
		if (v3_debug_vm(pisces_cmd.spec.vm_id, 
				pisces_cmd.spec.core, 
				pisces_cmd.spec.cmd) == -1) {
		    send_resp(pisces_fd, -1);
		    break;
		}
			    
		send_resp(pisces_fd, 0);
		break;
	    }
