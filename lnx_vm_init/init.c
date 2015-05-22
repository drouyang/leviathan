#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>


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
    hobbes_cmd_exit();

    hobbes_client_deinit();
}



int
main(int argc, char ** argv, char * envp[]) 
{
    hcq_handle_t hcq    = HCQ_INVALID_HANDLE;
    int          hcq_fd = 0;


    struct pollfd ufds[1] = {{-1, 0, 0}};



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
	    
	    ufds[1].fd     = hcq_fd;
	    ufds[1].events = POLLIN;
	    
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

	ret = poll(ufds, 1, -1);

	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}


	/* Handle Hobbes commands */
	if ( ( hobbes_enabled ) && 
	     ( ufds[1].revents & POLLIN ) ) {

	    ret = hobbes_handle_cmd(hcq);

	    if (ret == -1) {
		ERROR("Hobbes handler fault\n");
		continue;
		
	    }
	}
    }
    
    
    return 0;
}
