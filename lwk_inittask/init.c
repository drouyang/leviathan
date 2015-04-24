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

#include <pet_log.h>

#include <v3vee.h>

#include <hobbes.h>
#include <xemem.h>
#include <hobbes_enclave.h>
#include <hobbes_cmd_queue.h>

#include "init.h"
#include "pisces.h"
#include "pisces_ctrl.h"
#include "hobbes_ctrl.h"
#include "palacios.h"


cpu_set_t   enclave_cpus;
char      * enclave_name =  NULL; 


bool hobbes_enabled   = false;
bool palacios_enabled = false;

static void hobbes_exit( void ) {
    hobbes_client_deinit();
}



int
main(int argc, char ** argv, char * envp[]) 
{
    hcq_handle_t hcq = HCQ_INVALID_HANDLE;

    int hcq_fd    = 0;
    int pisces_fd = 0;

    struct pollfd ufds[2] = {{-1, 0, 0}, 
			     {-1, 0, 0}};


    CPU_ZERO(&enclave_cpus);	/* Initialize CPU mask */
    CPU_SET(0, &enclave_cpus);  /* We always boot on CPU 0 */

  
 
    printf("Pisces Control Daemon\n");

    /* Set up Pisces interface */
    {
	printf("Initializing Pisces Interface\n");
	if (pisces_init() != 0) {
	    ERROR("Could not initialize pisces interface\n");
	    return -1;
	}

	pisces_fd = pisces_get_fd();

	ufds[0].fd     = pisces_fd;
	ufds[0].events = POLLIN;
    }


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
	int    ret  = 0;

	ret = poll(ufds, 2, -1);

	if (ret == -1) {
	    ERROR("Select() error\n");
	    break;
	}

	/* Execute Legacy Pisces Commands if needed */
	if ( ufds[0].revents & POLLIN ) {

	    ret = pisces_handle_cmd(pisces_fd);

	    if (ret == -1) {
		ERROR("Pisces handler fault\n");
		continue;
	    }
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
