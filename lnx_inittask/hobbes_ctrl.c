/* Hobbes Enclave Commands 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#include <pet_log.h>
#include <pet_xml.h>
#include <pet_cpu.h>
#include <pet_mem.h>

#include <hobbes_enclave.h>
#include <hobbes_util.h>

#include <v3vee.h>

#include "init.h"
#include "hobbes_ctrl.h"
#include "lnx_app.h"
#include "file_io.h"
#include "palacios.h"

static hcq_handle_t hcq = HCQ_INVALID_HANDLE;

bool hobbes_enabled = false;


static int 
__handle_cmd(int    fd, 
  	     void * priv_data)
{
    hcq_handle_t  hcq      = (hcq_handle_t)priv_data;
    hcq_cmd_fn    handler  = NULL;
    hcq_cmd_t     cmd      = hcq_get_next_cmd(hcq);

    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Received invalid command\n");
	return -1;
    }

    handler  = hcq_get_cmd_handler(hcq, cmd);
    
    if (handler == NULL) {
	ERROR("Received invalid Hobbes command (%lu)\n", hcq_get_cmd_code(hcq, cmd));
	hcq_cmd_return(hcq, cmd, -1, 0, NULL);
	return -1;
    }
    
    return handler(hcq, cmd);
}


int
hobbes_register_cmd(uint64_t      cmd,
		    hobbes_cmd_fn handler_fn)
{
    return hcq_register_cmd(hcq, cmd, handler_fn);
}


static int
__launch_app(hcq_handle_t hcq, 
	     uint64_t     cmd)
{
    uint32_t   data_size = 0;
    char     * xml_str   = NULL;

    int ret = -1;
    
    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (xml_str == NULL) {
	ERROR("Could not read App spec\n");
	goto out;
    }

    ret = launch_hobbes_lnx_app(xml_str);

    printf("Returning from app launch (ret=%d)\n", ret);
out:
    hcq_cmd_return(hcq, cmd, ret, 0, NULL);
    return 0;
}

static int
__kill_app(hcq_handle_t hcq,
	   uint64_t     cmd)
{
    uint32_t      data_size = 0;
    hobbes_id_t * hpid      = NULL;

    int ret = -1;
    
    hpid = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (hpid == NULL) {
	ERROR("Could not read App spec\n");
	goto out;
    }

    ret = kill_hobbes_lnx_app(*hpid);

    printf("Returning from app kill (ret=%d)\n", ret);
out:
    hcq_cmd_return(hcq, cmd, ret, 0, NULL);
    return 0;

}

static int 
__ping(hcq_handle_t hcq,
       uint64_t     cmd)
{
    uint8_t * ping_data = NULL;
    uint32_t  data_size = 0;

    ping_data = hcq_get_cmd_data(hcq, cmd, &data_size);

    //    printf("Ping (Size=%u bytes)\n", data_size);

    hcq_cmd_return(hcq, cmd, 0, data_size, ping_data);

    return 0;
}

static int
__add_cpu(hcq_handle_t hcq,
	  uint64_t     cmd)
{
    uint32_t    data_size = 0;
    char      * xml_str   = NULL;
    pet_xml_t   xml       = NULL;
    char      * err_str   = NULL;

    uint32_t  cpu_id      = -1;
    uint32_t  apic_id     = -1;

    int       ret         = -1;

    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);
    
    if (xml_str == NULL) {
	err_str = "Could not read memory spec";
	goto out;
    }

    xml = pet_xml_parse_str(xml_str);

    if (xml == PET_INVALID_XML) {
	err_str = "Invalid XML syntax";
	goto out;
    }

    cpu_id  = smart_atou32(-1, pet_xml_get_val(xml, "phys_cpu_id" ));
    apic_id = smart_atou32(-1, pet_xml_get_val(xml, "apic_id" ));

    if (cpu_id == (uint32_t)-1) {
	err_str = "Invalid command syntax: invalid cpu_id";
	goto out;
    }

    if (apic_id == (uint32_t)-1) {
	err_str = "Invalid command syntax: invalid apic_id";
	goto out;
    }

    /* Online and lock the cpu */
    ret = pet_online_cpu(cpu_id);
    if (ret != 0) {
	err_str = "Error onlining cpu to Linux";
	goto out;
    }

    ret = pet_lock_cpu(cpu_id);
    if (ret != 0) {
	err_str = "Error locking cpu";
	pet_offline_cpu(cpu_id);
	goto out;
    }

    if (palacios_is_available()) {
	/* Notify Palacios of new cpu */
	ret = v3_add_cpu(cpu_id);
	if (ret == -1) {
	    err_str = "Error adding cpu to Palacios";
	    pet_unlock_cpu(cpu_id); 
	    pet_offline_cpu(cpu_id);
	    goto out;
	}

	ret = 0;
    }

 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0; 
}

static int
__add_memory(hcq_handle_t hcq,
	     uint64_t     cmd)
{
    uint32_t    data_size = 0;
    char      * xml_str   = NULL;
    pet_xml_t   xml       = NULL;
    char      * err_str   = NULL;

    uintptr_t base_addr   =  0;
    uint64_t  size        = -1;
    int       ret         = -1;

    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);
    
    if (xml_str == NULL) {
	err_str = "Could not read memory spec";
	goto out;
    }

    xml = pet_xml_parse_str(xml_str);

    if (xml == PET_INVALID_XML) {
	err_str = "Invalid XML syntax";
	goto out;
    }

    base_addr = smart_atou64(-1, pet_xml_get_val(xml, "base_addr" ));
    size      = smart_atou64(-1, pet_xml_get_val(xml, "size"      ));
    
    //if ((base_addr == -1) || (num_pgs == -1)) {
    if (base_addr == (uintptr_t)-1) {
	err_str = "Invalid command syntax";
	goto out;
    }

    /* Not totally sure here. For master memory, we online and lock, but eventually
     * we may want to handle VMs.      */
    while (size > 0) {
	uint32_t blk_index = pet_addr_to_block_id(base_addr);

	ret = pet_online_block(blk_index);
	if (ret != 0) {
	    err_str = "Error onlining memory block";
	    goto out;
	}

	ret = pet_lock_block(blk_index);
	if (ret != 0) {
	    err_str = "Error locking memory block";
	    pet_offline_block(blk_index);
	    goto out;
	}

	base_addr += pet_block_size();
	size      -= pet_block_size();
    }

    ret = 0;

 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0; 
}


static hcq_handle_t 
__hcq_init( void )
{
    hobbes_id_t   enclave_id = HOBBES_INVALID_ID;
    xemem_segid_t segid;
    char * hcq_name = NULL; 
    
    asprintf(&hcq_name, "%s-cmdq", hobbes_get_my_enclave_name());
	
    hcq = hcq_create_queue(hcq_name);
	
    free(hcq_name);
	
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not create command queue\n");
	return hcq;
    }
	
    segid = hcq_get_segid(hcq);
	
    enclave_id = hobbes_get_my_enclave_id();
	
    if (hobbes_register_enclave_cmdq(enclave_id, segid) != 0) {
	ERROR("Could not register command queue\n");
	hcq_free_queue(hcq);
	return HCQ_INVALID_HANDLE;
    }

    return hcq;
}


void 
hobbes_exit( void ) 
{
    printf("Shutting down hobbes\n");

    if (hcq != HCQ_INVALID_HANDLE) {
	hcq_free_queue(hcq);
    }

    if (hobbes_enabled) {
	if (!hobbes_is_master_inittask())
	    hobbes_client_deinit();
    }
}


int 
hobbes_init(void) 
{
    int hcq_fd = -1;

    if (!hobbes_is_master_inittask()) {
	if (hobbes_client_init() != 0) {
	    ERROR("Could not initialize hobbes client interface\n");
	    return -1;
	}
    }

    hobbes_enabled = true;
    //atexit(hobbes_exit);
    
    
    printf("\tHobbes Enclave: %s\n", hobbes_get_my_enclave_name());    

    printf("\tInitializing Hobbes Command Queue\n");
    hcq = __hcq_init();

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not initialize hobbes command queue\n");
	return -1;
    } 
	
    printf("\t...done\n");

    /* Register commands */
    hcq_register_cmd(hcq, HOBBES_CMD_APP_LAUNCH, __launch_app);
    hcq_register_cmd(hcq, HOBBES_CMD_APP_KILL,   __kill_app);
    hcq_register_cmd(hcq, HOBBES_CMD_PING,       __ping);
    hcq_register_cmd(hcq, HOBBES_CMD_ADD_CPU,    __add_cpu);
    hcq_register_cmd(hcq, HOBBES_CMD_ADD_MEM,    __add_memory);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_OPEN,  file_open_handler);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_CLOSE, file_close_handler);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_READ,  file_read_handler);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_WRITE, file_write_handler);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_STAT,  file_stat_handler);
    hcq_register_cmd(hcq, HOBBES_CMD_FILE_FSTAT, file_fstat_handler);

    /* Get File descriptor */    
    hcq_fd = hcq_get_fd(hcq);
    
    /* Register command handler */
    add_fd_handler(hcq_fd, __handle_cmd, hcq);

    /* Register that Hobbes userspace is running */
    hobbes_set_enclave_state(hobbes_get_my_enclave_id(), ENCLAVE_RUNNING);
    
    return 0;
}
