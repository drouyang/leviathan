
/* Hobbes Enclave Commands 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>


#include <pet_hashtable.h>
#include <pet_log.h>

#include <v3vee.h>

#include "hobbes_cmds.h"


static struct hashtable * hobbes_cmd_handlers = NULL;

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
    
    ret = handler(hcq, cmd);
    
    return ret;
}

static hcq_handle_t 
init_cmd_queue( void )
{
    hcq_handle_t  hcq = HCQ_INVALID_HANDLE;
    xemem_segid_t segid;

    hcq = hcq_create_queue();
    
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not create command queue\n");
	return hcq;
    }

    segid = hcq_get_segid(hcq);

    if (enclave_register_cmd_queue(enclave_name, segid) != 0) {
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

hcq_handle_t
hobbes_cmd_init(void)
{
    
    hobbes_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    
    
    

    return init_cmd_queue();
}





