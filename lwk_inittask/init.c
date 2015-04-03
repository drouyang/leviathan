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
#include "pisces_cmds.h"
#include "palacios.h"
#include "job_launch.h"


cpu_set_t   enclave_cpus;
char      * enclave_name =  NULL; 


static int pisces_fd = 0;

bool hobbes_enabled = true;
bool v3vee_enabled  = true;

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
main(int argc, char ** argv, char * envp[]) 
{
    hcq_handle_t hcq = HCQ_INVALID_HANDLE;

    int max_fd    = 0;
    int hcq_fd    = 0;

    fd_set full_set;

    FD_ZERO(&full_set);

    CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
    CPU_SET(0, &enclave_cpus);  /* We always boot on CPU 0 */

    hobbes_cmd_handlers = pet_create_htable(0, handler_hash_fn, handler_equal_fn);
 
    printf("Pisces Control Daemon\n");

    // Get Enclave Name

    enclave_name = getenv("ENCLAVE_NAME");

    if (enclave_name == NULL) {
	ERROR("Enclave Name is not set\n");
	hobbes_enabled = false;
    }

    /* Set up Pisces interface */
    {
	pisces_fd = open(PISCES_CTRL_PATH, O_RDWR);
	
	if (pisces_fd < 0) {
	    ERROR("Error opening pisces cmd file (%s)\n", PISCES_CTRL_PATH);
	    return -1;
	}
	
	pisces_cmd_init();

	FD_SET(pisces_fd, &full_set);
	max_fd = (max_fd < pisces_fd) ? pisces_fd + 1 : max_fd;

    }


    /* Set up Hobbes interface */
    if (hobbes_enabled) {
	printf("Hobbes Enclave: %s\n", enclave_name);
    
	hobbes_client_init();
    
	hcq = init_cmd_queue();
    
	/* Get File descriptors */    
	hcq_fd = hcq_get_fd(hcq);

	FD_SET(hcq_fd, &full_set);
	max_fd = (max_fd < hcq_fd) ? hcq_fd + 1 : max_fd;
    
    }
    
    /* Setup v3vee interface */
    if (v3vee_enabled) {

	// setup V3vee handlers 

    }

    while (1) {
	int    ret  = 0;
	fd_set rset = full_set;

	ret = select(max_fd, &rset, NULL, NULL, NULL);

	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}


	if ( FD_ISSET(pisces_fd, &rset) ) {

	    ret = pisces_handle_cmd(pisces_fd);

	    if (ret == -1) {
		ERROR("Pisces handler fault\n");
		return -1;
	    }

	}


	if ( ( hobbes_enabled ) && 
	     ( FD_ISSET(hcq_fd, &rset) ) ) {

	    hobbes_cmd_fn handler = NULL;

	    hcq_cmd_t cmd      = hcq_get_next_cmd(hcq);
	    uint64_t  cmd_code = hcq_get_cmd_code(hcq, cmd);
	    
	    printf("Hobbes cmd code=%llu\n", cmd_code);

	    handler = (hobbes_cmd_fn)pet_htable_search(hobbes_cmd_handlers, (uintptr_t)cmd_code);

	    if (handler == NULL) {
		ERROR("Received invalid Hobbes command (%llu)\n", cmd_code);
		hcq_cmd_return(hcq, cmd, -1, 0, NULL);
		continue;
	    }
	    
	    ret = handler(hcq, cmd_code);
	    
	    if (ret == -1) {
		ERROR("Hobbes handler fault\n");
		return -1;
	    }
	}
    }
    
    
    return 0;
 }
		




int 
register_hobbes_cmd(uint64_t        cmd, 
		    hobbes_cmd_fn   handler_fn)
{
    if (pet_htable_search(hobbes_cmd_handlers, cmd) != 0) {
	ERROR("Attempted to register duplicate command handler (cmd=%llu)\n", cmd);
	return -1;
    }
  
    if (pet_htable_insert(hobbes_cmd_handlers, cmd, (uintptr_t)handler_fn) == 0) {
	ERROR("Could not register hobbes command (cmd=%llu)\n", cmd);
	return -1;
    }

    return 0;

}


int
load_file(char * lnx_file, 
	  char * lwk_file)
{
	struct pisces_user_file_info * file_info = NULL;
	int    path_len  = strlen(lnx_file) + 1;
	size_t file_size = 0;
	char * file_buf  = NULL;

	file_info = calloc(1, sizeof(struct pisces_user_file_info) + path_len);
    
	file_info->path_len = path_len;
	strncpy(file_info->path, lnx_file, path_len - 1);
    
	file_size = ioctl(pisces_fd, PISCES_STAT_FILE, file_info);
    
	file_buf  = (char *)malloc(file_size);

	if (!file_buf) {
	    printf("Error: Could not allocate space for file (%s)\n", lnx_file);
	    return -1;
	}

	file_info->user_addr = (uintptr_t)file_buf;
	ioctl(pisces_fd, PISCES_LOAD_FILE, file_info);

	{
	    FILE * new_file = fopen(lwk_file, "w+");
	    
	    fwrite(file_buf, file_size, 1, new_file);

	    fclose(new_file);
	}

	free(file_buf);

	return 0;
}
