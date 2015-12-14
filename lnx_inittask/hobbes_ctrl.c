/* Hobbes Enclave Commands 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#include <pet_hashtable.h>
#include <pet_log.h>
#include <pet_xml.h>

#include <hobbes_enclave.h>

#include "init.h"
#include "hobbes_ctrl.h"
#include "lnx_app.h"
#include "file_io.h"

static hcq_handle_t hcq = HCQ_INVALID_HANDLE;

static struct hashtable * hobbes_cmd_handlers = NULL;

bool hobbes_enabled = false;


static uint32_t 
handler_hash_fn(uintptr_t key)
{
    return pet_hash_ptr(key);
}

static int
handler_equal_fn(uintptr_t key1, uintptr_t key2)
{
    return (key1 == key2);
}


static int 
__handle_cmd(int    fd, 
		  void * priv_data)
{
    hcq_handle_t  hcq      = (hcq_handle_t)priv_data;
    hobbes_cmd_fn handler  = NULL;
    hcq_cmd_t     cmd      = hcq_get_next_cmd(hcq);
    uint64_t      cmd_code = 0;

    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Received invalid command\n");
	return -1;
    }

    cmd_code = hcq_get_cmd_code(hcq, cmd);    

    //    printf("Hobbes cmd code=%lu\n", cmd_code);
    
    handler = (hobbes_cmd_fn)pet_htable_search(hobbes_cmd_handlers, (uintptr_t)cmd_code);
    
    if (handler == NULL) {
	ERROR("Received invalid Hobbes command (%lu)\n", cmd_code);
	hcq_cmd_return(hcq, cmd, -1, 0, NULL);
	return -1;
    }
    
    return handler(hcq, cmd);
}




int 
hobbes_register_cmd(uint64_t        cmd, 
		    hobbes_cmd_fn   handler_fn)
{
    if (pet_htable_search(hobbes_cmd_handlers, cmd) != 0) {
	ERROR("Attempted to register duplicate command handler (cmd=%lu)\n", cmd);
	return -1;
    }
  
    if (pet_htable_insert(hobbes_cmd_handlers, cmd, (uintptr_t)handler_fn) == 0) {
	ERROR("Could not register hobbes command (cmd=%lu)\n", cmd);
	return -1;
    }

    return 0;

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

    /* Track command handlers */
    hobbes_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    
    if (hobbes_cmd_handlers == NULL) {
	ERROR("Could not create hobbes command hashtable\n");
	return -1;
    }

    /* Register commands */
    hobbes_register_cmd(HOBBES_CMD_APP_LAUNCH, __launch_app);
    hobbes_register_cmd(HOBBES_CMD_PING,       __ping);
    hobbes_register_cmd(HOBBES_CMD_FILE_OPEN,  file_open_handler);
    hobbes_register_cmd(HOBBES_CMD_FILE_CLOSE, file_close_handler);
    hobbes_register_cmd(HOBBES_CMD_FILE_READ,  file_read_handler);
    hobbes_register_cmd(HOBBES_CMD_FILE_WRITE, file_write_handler);
    hobbes_register_cmd(HOBBES_CMD_FILE_STAT,  file_stat_handler);
    hobbes_register_cmd(HOBBES_CMD_FILE_FSTAT, file_fstat_handler);
    

    printf("\tInitializing Hobbes Command Queue\n");

    hcq = __hcq_init();

    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not initialize hobbes command queue\n");
	pet_free_htable(hobbes_cmd_handlers, 0, 0);
	return -1;
    } 
	
    printf("\t...done\n");
    
    /* Get File descriptor */    
    hcq_fd = hcq_get_fd(hcq);
    
    /* Register command handler */
    add_fd_handler(hcq_fd, __handle_cmd, hcq);

    /* Register that Hobbes userspace is running */
    hobbes_set_enclave_state(hobbes_get_my_enclave_id(), ENCLAVE_RUNNING);
    
    return 0;
}
