#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include <signal.h>

#include <stdint.h>

#include <pet_log.h>

#include <hobbes.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <hobbes_cmd_queue.h>

#include "hobbes_ctrl.h"


char * enclave_name   =  NULL; 
bool   hobbes_enabled = false;



static void hobbes_exit( void ) {
    if (hobbes_enabled) {
	printf("Shutting down hobbes\n");
	hobbes_cmd_exit();
	hobbes_client_deinit();
    }
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
    hcq_handle_t hcq    = HCQ_INVALID_HANDLE;
    int          hcq_fd = 0;
    fd_set       cmd_fds;


    {
	struct sigaction action;
	
	memset(&action, 0, sizeof(struct sigaction));
	action.sa_handler = sig_term_handler;

	if (sigaction(SIGINT, &action, 0)) {
	    perror("sigaction");
	    return -1;
	}
    }

    FD_ZERO(&cmd_fds);

    printf("Checking for Hobbes environment...\n");
    

    /* Set up Hobbes interface */
    if (hobbes_is_available()) {
    
	if (hobbes_client_init() != 0) {
	    ERROR("Could not initialize hobbes client interface\n");
	    goto hobbes_init_out;
	}

	atexit(hobbes_exit);


	printf("\tHobbes Enclave: %s\n", hobbes_get_my_enclave_name());
   
	printf("\tInitializing Hobbes Command Queue\n");

	hcq = hobbes_cmd_init();

	if (hcq == HCQ_INVALID_HANDLE) {
	    ERROR("Could not initialize hobbes command queue\n");
	    ERROR("Running in a degraded state with legacy pisces interface\n");
	    goto hobbes_init_out;
	} else {

	    printf("\t...done\n");
	    
	    /* Get File descriptors */    
	    hcq_fd = hcq_get_fd(hcq);
	    
	    FD_SET(hcq_fd, &cmd_fds);	    
	    
	    /* Register that Hobbes userspace is running */
	    hobbes_set_enclave_state(hobbes_get_my_enclave_id(), ENCLAVE_RUNNING);

	    hobbes_enabled = true;
	}
    }
 hobbes_init_out:

    if (!hobbes_enabled) {
	printf("Hobbes is not available. Exitting.\n");
	exit(-1);
    }

    /* Command Loop */
    printf("Entering Command Loop\n");
    while (1) {
	int    ret  = 0;	
	fd_set rset = cmd_fds;

	ret = select(hcq_fd + 1, &rset, NULL, NULL, NULL);

	printf("select returned\n");
	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}


	/* Handle Hobbes commands */
	if ( ( hobbes_enabled ) && 
	     ( FD_ISSET(hcq_fd, &rset)) ) {

	    ret = hobbes_handle_cmd(hcq);

	    if (ret == -1) {
		ERROR("Hobbes handler fault\n");
		continue;
		
	    }
	}
    }
    
    
    return 0;
}
