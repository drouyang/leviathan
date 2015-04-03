#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <lwk/liblwk.h>
#include <arch/types.h>
#include <lwk/pmem.h>
#include <lwk/smp.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <poll.h>

#include <stdint.h>

#include <xemem.h>
#include <pet_log.h>
#include <pet_hashtable.h>
#include <cmd_queue.h>

#include "init.h"
#include "pisces.h"
#include "palacios.h"
#include "job_launch.h"


cpu_set_t   enclave_cpus;
uint64_t    enclave_id = -1;


static struct hashtable * cmd_handlers        = NULL;
static struct hashtable * legacy_cmd_handlers = NULL;

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

struct cmd_handler {
    uint64_t         cmd;
    cmd_handler_fn   handler_fn;
    void           * priv_data;
};


int
main(int argc, char ** argv, char * envp[]) 
{
    hcq_handle_t hcq = HCQ_INVALID_HANDLE;
    fd_set       rset;    

    int hcq_fd    = 0;
    int pisces_fd = 0;
    int max_fd    = 0;


    CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
    CPU_SET(0, &enclave_cpus);  /* We always boot on CPU 0 */


    printf("Pisces Control Daemon\n");

    // Get Enclave ID

    // register handlers
    cmd_handlers        = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    legacy_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
    
    hobbes_client_init();
    
    hcq = hcq_create_queue();
    
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not create command queue\n");
	return -1;
    }

    // register command queue w/ enclave
    
    

    while (1) {
	int ret = 0;
	
	
	//printf("Command=%llu, data_len=%d\n", cmd.cmd, cmd.data_len);


    }
    
    
    return 0;
 }
		




int 
register_cmd_handler(uint64_t         cmd, 
		     cmd_handler_fn   handler_fn, 
		     void           * priv_data)
{
    struct cmd_handler * handler = NULL;

    if (pet_htable_search(cmd_handlers, cmd) != 0) {
	ERROR("Attempted to register duplicate command handler (cmd=%llu)\n", cmd);
	return -1;
    }

    handler = calloc(1, sizeof(struct cmd_handler));
    
    handler->cmd        = cmd;
    handler->handler_fn = handler_fn;
    handler->priv_data  = priv_data;

    if (pet_htable_insert(cmd_handlers, cmd, (uintptr_t)handler) == 0) {
	ERROR("Could not register command (cmd=%llu)\n", cmd);
	free(handler);
	return -1;
    }

    return 0;

}
