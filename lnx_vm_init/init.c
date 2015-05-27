/* Leviathan Linux inittask 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include <signal.h>

#include <stdint.h>
#include <assert.h>

#include <pet_log.h>
#include <pet_hashtable.h>

#include <hobbes.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <hobbes_cmd_queue.h>

#include "hobbes_ctrl.h"
#include "lnx_app.h"
#include "init.h"


static uint32_t           handler_max_fd = 0;
static fd_set             handler_fdset;
static struct hashtable * handler_table  = NULL;

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




static void
sig_term_handler(int sig)
{
    printf("Caught sigterm\n");	
    exit(-1);
}



int
main(int argc, char ** argv, char * envp[]) 
{

    /* Trap SIGINT for cleanup */
    {
	struct sigaction action;
	
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = sig_term_handler;
	
	if (sigaction(SIGINT, &action, 0)) {
	    perror("sigaction");
	    return -1;
	}
    }
    


    handler_table = pet_create_htable(0, handler_hash_fn, handler_equal_fn);

    if (handler_table == NULL) {
	ERROR("Could not create FD handler hashtable\n");
	return -1;
    }



    /* Check if hobbes is available */
    printf("Checking for Hobbes environment...\n");

    if (!hobbes_is_available()) {
	printf("Hobbes is not available. Exitting.\n");
	exit(-1);
    }    

    if (hobbes_init() == -1) {
	ERROR("Could not initialize hobbes environment\n");
	exit(-1);
    }


    /* Command Loop */
    printf("Entering Command Loop\n");

    while (1) {
	int    i    = 0;
	int    ret  = 0;	
	fd_set rset = handler_fdset;

	ret = select(handler_max_fd + 1, &rset, NULL, NULL, NULL);

	printf("select returned\n");
	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}


	for (i = 0; i <= handler_max_fd; i++) {
	    if (FD_ISSET(i, &handler_fdset)) {
		struct fd_handler * handler = NULL;
		int ret = 0;

		handler = (struct fd_handler *)pet_htable_search(handler_table, i);

		if (handler == NULL) {
		    ERROR("FD is set, but there is not handler associated with it...\n");
		    continue;
		}
		
		assert(handler->fd == i);

		ret = handler->fn(i, handler->priv_data);
		
		if (ret != 0) {
		    ERROR("Error in fd handler for FD (%d)\n", i);
		    ERROR("Removing FD from handler set\n");
		    FD_CLR(i, &handler_fdset);
		    continue;
		}

	    }
	    
	}

   }
    
    
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

    if (pet_htable_insert(handler_table, fd, (uintptr_t)new_handler) == 0) {
	ERROR("Could not register FD handler (fd=%d)\n", fd);
	free(new_handler);
	return -1;
    }

    FD_SET(fd, &handler_fdset);

    if (handler_max_fd < fd) handler_max_fd = fd;

    return 0;
}


int
remove_fd_handler(int fd)
{
    struct fd_handler * handler = NULL;

    FD_CLR(fd, &handler_fdset);
    
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

    return 0;
}
