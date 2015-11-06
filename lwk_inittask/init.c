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
#include <assert.h>


#include <stdint.h>

#include <pet_log.h>
#include <pet_hashtable.h>

#include <v3vee.h>

#include <hobbes.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <hobbes_cmd_queue.h>
#include <hobbes_util.h>

#include "init.h"
#include "pisces.h"
#include "pisces_ctrl.h"
#include "hobbes_ctrl.h"
#include "palacios.h"


cpu_set_t   enclave_cpus;
char      * enclave_name =  NULL; 


static struct hashtable * handler_table = NULL;
static struct pollfd    * handler_fds   = NULL;
static uint32_t           handler_cnt   = 0;

static int update_poll_fds( void );


struct fd_handler {
    int             fd;
    fd_handler_fn   fn;
    void          * priv_data;
};


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



bool hobbes_enabled   = false;
bool palacios_enabled = false;



int
main(int argc, char ** argv, char * envp[]) 
{

    CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
    CPU_SET(0, &enclave_cpus);  /* We always boot on CPU 0 */

  
   /* Initialize command table */
    handler_table = pet_create_htable(0, handler_hash_fn, handler_equal_fn);

    if (handler_table == NULL) {
	ERROR("Could not create FD handler hashtable\n");
	return -1;
    }
 

    printf("Pisces Control Daemon\n");

    /* Set up Pisces interface */
    {
	printf("Initializing Pisces Interface\n");
	if (pisces_init() != 0) {
	    ERROR("Could not initialize pisces interface\n");
	    return -1;
	}
    }



    /* Set up Hobbes interface */
    printf("Checking for Hobbes environment...\n");
    if (hobbes_is_available()) {
	if (hobbes_init() != 0) {
	    ERROR("Could not initialize Hobbes interface\n");
	    goto hobbes_init_out;
	}
	
	hobbes_enabled = true;
    }
 hobbes_init_out:
    
    /* Setup v3vee interface */
    printf("Checking for Palacios...\n");
    if (palacios_is_available()) {

	if (palacios_init() != 0) {
	    ERROR("Could not initialize Palacios interface\n");
	    goto palacios_init_out;
	}

	palacios_enabled = true;
    }
 palacios_init_out:



    printf("Hobbes:   %s\n", (hobbes_enabled   ?  "ENABLED" : "DISABLED"));
    printf("Palacios: %s\n", (palacios_enabled ?  "ENABLED" : "DISABLED"));

    /* Command Loop */
    printf("Entering Command Loop\n");
    while (1) {
	int      ret  = 0;
	uint32_t i    = 0;

	ret = poll(handler_fds, handler_cnt, -1);

	if (ret == -1) {
	    ERROR("poll() error\n");
	    break;
	}

	
	for (i = 0; i < handler_cnt; i++) {
	    if (handler_fds[i].revents & POLLIN) {
		struct fd_handler * handler = NULL;

		int fd = handler_fds[i].fd;
		
		handler = (struct fd_handler *)pet_htable_search(handler_table, fd);

		if (handler == NULL) {
		    ERROR("FD is set, but there is no handler associated with it...\n");
		    continue;
		}

		assert(handler->fd == fd);

		ret = handler->fn(fd, handler->priv_data);
		
		if (ret != 0) {
		    ERROR("Error in fd handler for FD (%d)\n", i);
		    ERROR("Removing FD from handler set\n");
		    handler_fds[i].fd = -1;
		    continue;
		}
	    }
	}
    }
    
    
    return 0;
}
		

static int
update_poll_fds(void)
{
    struct hashtable_iter * iter = NULL;

    int num_fds = pet_htable_count(handler_table);
    int i       = 0;

    /* Free the old fd array */
    if (handler_fds) {
	smart_free(handler_fds);
	handler_cnt = 0;
    }


    /* No fds to watch */
    if (num_fds <= 0) {
	return 0;
    }

    handler_fds = calloc(sizeof(struct pollfd), num_fds);

    if (handler_fds == NULL) {
	ERROR("Could not allocate pollfd array\n");
	return -1;
    }

    iter = pet_htable_create_iter(handler_table);

    if (iter == NULL) {
	ERROR("Could not create fd iterator\n");
	smart_free(handler_fds);
	return -1;
    }

    for (i = 0; i < num_fds; i++) {
	struct fd_handler * tmp_handler = NULL;
	
	tmp_handler = (struct fd_handler *)pet_htable_get_iter_value(iter);

	if (tmp_handler == NULL) {
	    ERROR("FD handler hashtable corrupt.\n");
	    smart_free(handler_fds);
	    return -1;
	}

	handler_fds[i].fd     = tmp_handler->fd;
	handler_fds[i].events = POLLIN;

	/* Probably should check if count and iterators are inconsistent */
	pet_htable_iter_advance(iter);	
    }

    handler_cnt = num_fds;

    return 0;
}

int
add_fd_handler(int             fd,
	       fd_handler_fn   fn,
	       void          * priv_data)
{
    struct fd_handler * new_handler = NULL;

    if (pet_htable_search(handler_table, fd) != 0) {
	ERROR("Attempted to register a duplicate FD handler (fd=%d)\n", fd);
	return -1;
    }

    new_handler = calloc(sizeof(struct fd_handler), 1);
    
    if (new_handler == NULL) {
	ERROR("Could not allocate FD handler state\n");
	return -1;
    }

    new_handler->fn        = fn;
    new_handler->fd        = fd;
    new_handler->priv_data = priv_data;

    if (pet_htable_insert(handler_table, fd, (uintptr_t)new_handler) == 0) {
	ERROR("Could not register FD handler (fd=%d)\n", fd);
	free(new_handler);
	return -1;
    }

    /* Add FD to poll list */
    if (update_poll_fds() != 0) {
	smart_free(new_handler);
	return -1;
    }
	
    return 0;
}


int
remove_fd_handler(int fd)
{
    struct fd_handler * handler = NULL;
  
    handler = (struct fd_handler *)pet_htable_search(handler_table, fd);

    if (handler == NULL) {
	ERROR("Could not find handler for FD (%d)\n", fd);
	return -1;
    }

    handler = (struct fd_handler *)pet_htable_remove(handler_table, fd, 0);

    if (handler == NULL) {
	ERROR("Could not remove handler from the FD hashtable (fd=%d)\n", fd);
	return -1;
    }

    free(handler);

    /* Remove FD to poll list */
    if (update_poll_fds() != 0) {
	return -1;
    }

    return 0;
}



int 
load_remote_file(char * remote_file,
		 char * local_file)
{
    size_t file_size = 0;
    char * file_buf  = NULL;

    file_size = pisces_file_stat(remote_file);

    file_buf  = (char *)malloc(file_size);

    if (!file_buf) {
	printf("Error: Could not allocate space for file (%s)\n", remote_file);
	return -1;
    }

    pisces_file_load(remote_file, file_buf);

    {
	FILE * new_file = fopen(local_file, "w+");
	    
	fwrite(file_buf, file_size, 1, new_file);

	fclose(new_file);
    }

    free(file_buf);

    return 0;
}
