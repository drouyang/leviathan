#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>


#include <hobbes.h>
#include <hobbes_cmd_queue.h>
#include <hobbes_db.h>
#include <hobbes_util.h>

#include <v3vee.h>
#include <v3_ioctl.h>

#include <pet_hashtable.h>
#include <pet_ioctl.h>
#include <pet_log.h>
#include <pet_xml.h>

#include "palacios.h"
#include "hobbes_ctrl.h"
#include "pisces_ctrl.h"
#include "init.h"


struct hobbes_vm_cons_connect_info {
    /* Enclave ID of VM */
    hobbes_id_t   enclave_id;

    /* Signallable segment for kicking reader */
    xemem_segid_t kick_segid;

    /* Number of pages in ring buffer */
    unsigned long num_pages;
} __attribute__((packed));

struct hobbes_vm_cons_state {
    /* Ring buffer memory */
    size_t             ring_buf_size;
    struct pmem_region ring_buf_pmem;
    vaddr_t            ring_buf_va;

    /* Segid of ring buf */ 
    xemem_segid_t ring_buf_segid;

    /* Apid to kick reader */
    xemem_apid_t  kick_apid;

    /* fd for console */
    int           cons_fd;

    /* VM id */
    int           vm_id;
} __attribute__((packed));


struct hobbes_vm_cons_keycode {
    /* Enclave ID of VM */
    hobbes_id_t   enclave_id;

    /* scancode */
    unsigned char scan_code;
} __attribute__((packed));



static struct hashtable * hobbes_cons_state_table;
extern hdb_db_t           hobbes_master_db;







static int
__hobbes_launch_vm(hcq_handle_t hcq,
		   hcq_cmd_t    cmd)
{
    hobbes_id_t enclave_id     = -1;
    char      * enclave_id_str = NULL;

    pet_xml_t   xml            =  NULL;
    char      * xml_str        =  NULL;
    uint32_t    data_size      =  0;

    char      * enclave_name   =  NULL;    
    int         vm_id          = -1;

    int         ret            = -1;
    char      * err_str        = NULL;


    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if ((xml_str == NULL) || (xml_str[data_size - 1] != '\0')) {
	err_str = "Received invalid command string";
	goto out2;
    }

    xml = pet_xml_parse_str(xml_str);

    if (!xml) {
	err_str = "Invalid VM Spec";
	goto out2;
    }

    enclave_name = pet_xml_get_val(xml, "name");

    if (enclave_name == NULL) {
	err_str = "Invalid VM Spec. Missing \'name\' field\n";
	goto out2;
    }
    
    enclave_id_str = pet_xml_get_val(xml, "enclave_id");
    
    if (enclave_id_str == NULL) {
	err_str = "Invalid VM Spec. Missing \'enclave_id\' field\n";
	goto out2;
    }

    enclave_id = smart_atoi(-1, enclave_id_str);
 
    if (enclave_id == -1) {
	err_str = "Invalid VM Spec. Invalid \'enclave_id\' field";
	goto out2;
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

    if (type != VM_ENCLAVE) {
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



static int
__hobbes_alloc_ring_buf(struct hobbes_vm_cons_state * state)
{
    int  status;
    id_t aspace_id;

    status = pmem_alloc_umem(state->ring_buf_size, PAGE_SIZE, &(state->ring_buf_pmem));
    if (status != 0) {
	ERROR("Could not alloc pmem for ring buffer (status=%d)\n", status);
	return status;
    }

    aspace_get_myid(&aspace_id);

    status = aspace_map_region_anywhere(
		aspace_id,
		&(state->ring_buf_va),
		state->ring_buf_size,
		(VM_USER | VM_READ | VM_WRITE),
		PAGE_SIZE,
		"Hobbes_VM_Console",
		state->ring_buf_pmem.start);
    if (status != 0) {
	ERROR("Could not map pmem to aspace (status=%d)\n", status);
	pmem_free_umem(&(state->ring_buf_pmem));
	return status;
    }

    return 0;
}

static void
__hobbes_free_ring_buf(struct hobbes_vm_cons_state * state)
{
    id_t aspace_id;
    aspace_get_myid(&aspace_id);

    aspace_unmap_region(aspace_id, state->ring_buf_va, state->ring_buf_size);
    pmem_free_umem(&(state->ring_buf_pmem));
}

static int
__hobbes_cons_event(int    fd,
                    void * private_data)
{
    struct hobbes_vm_cons_state * state = private_data;

    assert(state->cons_fd == fd);

    /* We don't actually read anything - just kick the reader side which maps the console
     * directly
     */

    xemem_signal(state->kick_apid);
    return 0;
}

static int
__hobbes_vm_cons_connect(hcq_handle_t hcq,
			 hcq_cmd_t    cmd)
{
    enclave_type_t type      =  INVALID_ENCLAVE;
    hobbes_id_t    parent_id =  HOBBES_INVALID_ID;
    uint32_t       data_size =  0;
    int            status    =  0;
    int64_t        ret       = -1;
    char         * err_str   =  NULL;

    struct hobbes_vm_cons_connect_info * connect_info = NULL;
    struct hobbes_vm_cons_state        * state        = NULL; 
    struct v3_hobbes_console_info        cons_info;

    state = malloc(sizeof(struct hobbes_vm_cons_state));
    if (state == NULL) {
	err_str = "Out of memory";
	goto out;
    }

    connect_info = hcq_get_cmd_data(hcq, cmd, &data_size);
    if (data_size != sizeof(struct hobbes_vm_cons_connect_info)) {
	err_str = "Ccrrupt connection info";
	goto out_cmd;
    } 
   
    /* Double check the enclave type is correct */
    type = hobbes_get_enclave_type(connect_info->enclave_id);
    if (type != VM_ENCLAVE) {
	err_str = "Enclave is not a VM";
	goto out_cmd;
    }

    /* Double check that the VM is hosted by us */
    parent_id = hobbes_get_enclave_parent(connect_info->enclave_id);
    if (parent_id != hobbes_get_my_enclave_id()) {
	err_str = "VM is not hosted by this enclave";
	goto out_cmd;
    }

    /* Get local VM ID */
    state->vm_id = hobbes_get_enclave_dev_id(connect_info->enclave_id);
    if (state->vm_id == -1) {
	err_str = "Could not find VM instance";
	goto out_cmd;
    }

    /* Ensure we're not already connected */
    if (pet_htable_search(hobbes_cons_state_table, (uintptr_t)state->vm_id)) {
	err_str = "VM console already connected";
	goto out_cmd;
    }

    /* Allocate the ring buffer */
    state->ring_buf_size = connect_info->num_pages * PAGE_SIZE;
    status = __hobbes_alloc_ring_buf(state);
    if (status != 0) {
	err_str = "Could not allocate ring buffer";
	goto out_cmd;
    }

    /* XEMEM make it */
    memset((void *)state->ring_buf_va, 0, state->ring_buf_size);
    state->ring_buf_segid = xemem_make((void *)state->ring_buf_va, state->ring_buf_size, NULL);
    if (state->ring_buf_segid == XEMEM_INVALID_SEGID) {
	err_str = "Could not export ring buffer via XEMEM";
	goto out_export;
    }

    /* Get the segid to kick events with */
    state->kick_apid = xemem_get(connect_info->kick_segid, XEMEM_RDWR);
    if (state->kick_apid < 0) {
	err_str = "Could not XEMEM_GET client segid";
	goto out_get;
    }

    /* Connect the console */
    cons_info.ring_buf_pa = (uintptr_t)state->ring_buf_pmem.start;
    cons_info.num_pages   = (u32)connect_info->num_pages;
    status = __issue_vm_cmd(state->vm_id, V3_VM_CONSOLE_CONNECT, (uintptr_t)&cons_info);
    if (status!= 0) {
	err_str = "Could not connect VM console";
	goto out_connect;
    }

    state->cons_fd = cons_info.cons_fd;

    /* Add fd to the poll list */
    status = add_fd_handler(state->cons_fd, __hobbes_cons_event, state);
    if (status != 0) {
	err_str = "Corrupted fd handler interface";
	goto out_fd;
    }

    /* Update the htable */
    status = pet_htable_insert(hobbes_cons_state_table, (uintptr_t)state->vm_id,
		(uintptr_t)state);
    if (status == 0) {
	err_str = "Corrupted hashtable";
	goto out_htable;
    }

    /* Success */
    printf("Successfully connected to Palacios VM console\n"); 
    ret = 0;
    goto out;

 out_htable:
    remove_fd_handler(state->cons_fd);

 out_fd:
    close(state->cons_fd);

 out_connect:
    xemem_release(state->kick_apid);

 out_get:
    xemem_remove(state->ring_buf_segid);

 out_export:
    __hobbes_free_ring_buf(state);

 out_cmd:
    free(state);

 out:
    if (err_str) ERROR("%s\n", err_str);

    /* On success, return the ring buf segid as return code */
    if (ret == 0)
	ret = state->ring_buf_segid;

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;
}


static struct hobbes_vm_cons_state *
__hobbes_get_or_remove_vm_cons_state(hobbes_id_t  enclave_id,
				     char      ** err_str,
				     int	  remove)
{
    enclave_type_t type      =  INVALID_ENCLAVE;
    hobbes_id_t    parent_id =  HOBBES_INVALID_ID;
    int            vm_id     = -1;

    struct hobbes_vm_cons_state * state = NULL; 

    *err_str = NULL;

    /* Double check the enclave type is correct */
    type = hobbes_get_enclave_type(enclave_id);
    if (type != VM_ENCLAVE) {
	*err_str = "Enclave is not a VM";
	return NULL;
    }

    /* Double check that the VM is hosted by us */
    parent_id = hobbes_get_enclave_parent(enclave_id);
    if (parent_id != hobbes_get_my_enclave_id()) {
	*err_str = "VM is not hosted by this enclave";
	return NULL;
    }

    /* Get local VM ID */
    vm_id = hobbes_get_enclave_dev_id(enclave_id);
    if (vm_id == -1) {
	*err_str = "Could not find VM instance";
	return NULL;
    }

    /* Get state from htable */
    if (remove) {
	state = (struct hobbes_vm_cons_state *)pet_htable_remove(
		hobbes_cons_state_table, 
		(uintptr_t)vm_id, 
		0);
    } else {
	state = (struct hobbes_vm_cons_state *)pet_htable_search(
		hobbes_cons_state_table, 
		(uintptr_t)vm_id); 
    }

    if (state == NULL) {
	*err_str = "No console state for VM";
	return NULL;
    }

    return state;
}

static struct hobbes_vm_cons_state *
__hobbes_get_vm_cons_state(hobbes_id_t  enclave_id,
                           char      ** err_str)
{
    return __hobbes_get_or_remove_vm_cons_state(enclave_id, err_str, 0);
}

static struct hobbes_vm_cons_state *
__hobbes_remove_vm_cons_state(hobbes_id_t  enclave_id,
                              char      ** err_str)
{
    return __hobbes_get_or_remove_vm_cons_state(enclave_id, err_str, 1);
}

static int
__hobbes_vm_cons_disconnect(hcq_handle_t hcq,
			    hcq_cmd_t    cmd)
{
    hobbes_id_t * enclave_id =  NULL;
    char	* err_str    =  NULL;
    uint32_t      data_size  =  0;
    int           ret        = -1;

    struct hobbes_vm_cons_state * state = NULL;

    enclave_id = hcq_get_cmd_data(hcq, cmd, &data_size);
    if (data_size != sizeof(hobbes_id_t)) {
	err_str = "Corrupt disconnection info";
	goto out;
    } 

    state = __hobbes_remove_vm_cons_state(*enclave_id, &err_str);
    if (state == NULL) 
	goto out;


    /* Perform teardown */
    remove_fd_handler(state->cons_fd);
    close(state->cons_fd);
    xemem_release(state->kick_apid);
    xemem_remove(state->ring_buf_segid);
    __hobbes_free_ring_buf(state);
    free(state);

    ret = 0;

    printf("Disconnected from Palacios VM console\n"); 
 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;
}

static int
__hobbes_vm_cons_keycode(hcq_handle_t hcq,
			 hcq_cmd_t    cmd)
{
    char   * err_str    =  NULL;
    uint32_t data_size  =  0;
    int      ret        = -1;

    struct hobbes_vm_cons_state   * state    =  NULL;
    struct hobbes_vm_cons_keycode * key_code = NULL;

    key_code = hcq_get_cmd_data(hcq, cmd, &data_size);
    if (data_size != sizeof(struct hobbes_vm_cons_keycode)) {
	err_str = "Corrupt keycode info";
	goto out;
    } 

    state = __hobbes_get_vm_cons_state(key_code->enclave_id, &err_str);
    if (state == NULL) 
	goto out;

    /* Write to VM console */
    ret = write(state->cons_fd, &(key_code->scan_code), 1);
    if (ret != 1) {
	err_str = "Cannot deliver scancode to VM console";
	ret     = -1;
	goto out;
    }

    ret = 0;

 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;
}


static uint32_t
hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
eq_fn(uintptr_t key1,
      uintptr_t key2)
{
    return (key1 == key2);
}

int
palacios_init(void)
{

    // Register Hobbes commands
    if (hobbes_enabled) {
	hobbes_register_cmd(HOBBES_CMD_VM_LAUNCH,           __hobbes_launch_vm  );
	hobbes_register_cmd(HOBBES_CMD_VM_DESTROY,          __hobbes_destroy_vm );
	hobbes_register_cmd(ENCLAVE_CMD_VM_CONS_CONNECT,    __hobbes_vm_cons_connect);
	hobbes_register_cmd(ENCLAVE_CMD_VM_CONS_DISCONNECT, __hobbes_vm_cons_disconnect);
	hobbes_register_cmd(ENCLAVE_CMD_VM_CONS_KEYCODE,    __hobbes_vm_cons_keycode);

	hobbes_cons_state_table = pet_create_htable(0, hash_fn, eq_fn);
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
