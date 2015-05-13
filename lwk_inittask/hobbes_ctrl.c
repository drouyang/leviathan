
/* Hobbes Enclave Commands 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#include <pet_hashtable.h>
#include <pet_log.h>
#include <pet_xml.h>

#include <v3vee.h>

#include <hobbes_enclave.h>

#include "init.h"
#include "hobbes_ctrl.h"
#include "app_launch.h"



static hcq_handle_t hcq = HCQ_INVALID_HANDLE;

static struct hashtable * hobbes_cmd_handlers = NULL;


static void hcq_exit( void ) {
    printf("Freeing Hobbes Command Queue\n");
    hcq_free_queue(hcq);
}

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


int 
hobbes_handle_cmd(hcq_handle_t hcq)
{
    hobbes_cmd_fn handler  = NULL;
    hcq_cmd_t     cmd      = hcq_get_next_cmd(hcq);
    uint64_t      cmd_code = hcq_get_cmd_code(hcq, cmd);    

    printf("Hobbes cmd code=%lu\n", cmd_code);
    
    handler = (hobbes_cmd_fn)pet_htable_search(hobbes_cmd_handlers, (uintptr_t)cmd_code);
    
    if (handler == NULL) {
	ERROR("Received invalid Hobbes command (%lu)\n", cmd_code);
	hcq_cmd_return(hcq, cmd, -1, 0, NULL);
	return -1;;
    }
    
    return handler(hcq, cmd);
}

static hcq_handle_t 
init_cmd_queue( void )
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

    atexit(hcq_exit);

    segid = hcq_get_segid(hcq);

    enclave_id = hobbes_get_my_enclave_id();
 
    if (hobbes_register_enclave_cmdq(enclave_id, segid) != 0) {
	ERROR("Could not register command queue\n");
	hcq_free_queue(hcq);
	return HCQ_INVALID_HANDLE;
    }

    return hcq;
}


int 
register_hobbes_cmd(uint64_t        cmd, 
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

    /* ensure null termination */
    xml_str[data_size] = '\0';

    ret = launch_hobbes_lwk_app(xml_str);

 out:
    hcq_cmd_return(hcq, cmd, ret, 0, NULL);
    return 0;
}


static int
__load_file(hcq_handle_t hcq,
	    uint64_t     cmd)
{
    uint32_t   data_size = 0;
    char     * xml_str   = NULL;
    pet_xml_t  xml       = NULL;

    char     * src_file  = NULL;
    char     * dst_file  = NULL;

    int ret = -1;

    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (xml_str == NULL) {
	ERROR("Could not read File spec\n");
	goto out;
    }

    /* ensure null termination */
    xml_str[data_size] = '\0';

    xml = pet_xml_parse_str(xml_str);

    src_file = pet_xml_get_val(xml, "src_file");
    dst_file = pet_xml_get_val(xml, "dst_file");
    
    if ((src_file == NULL) || (dst_file == NULL)) {
	ERROR("Invalid File spec\n");
 	goto out;
    }
    
    ret = load_remote_file(src_file, dst_file);
    
 out:
    hcq_cmd_return(hcq, cmd, ret, 0, NULL);
    return 0;
}

hcq_handle_t
hobbes_cmd_init(void)
{
    
    hobbes_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    
    
    register_hobbes_cmd(HOBBES_CMD_APP_LAUNCH, __launch_app);
    register_hobbes_cmd(HOBBES_CMD_LOAD_FILE,  __load_file);

    return init_cmd_queue();
}





