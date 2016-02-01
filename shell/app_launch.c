#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <ctype.h>
#include <assert.h>
#include <stdint.h>
#include <signal.h>
#include <errno.h>

#include <hobbes_enclave.h>
#include <hobbes_app.h>
#include <hobbes_db.h>
#include <hobbes_util.h>
#include <hobbes_notifier.h>

#include <pet_log.h>

#include "hio.h"


#define DEFAULT_NUM_RANKS       1
#define DEFAULT_CPU_LIST        NULL
#define DEFAULT_USE_LARGE_PAGES 0
#define DEFAULT_USE_SMARTMAP    0
#define DEFAULT_HEAP_SIZE       (16 * 1024 * 1024)
#define DEFAULT_STACK_SIZE      (256 * 1024)
#define DEFAULT_ENVP            ""
#define DEFAULT_HIO_ARGV	NULL
#define DEFAULT_HIO_ENVP	NULL
#define DEFAULT_HIO_ENCLAVE	"master"
#define DEFAULT_HIO_NUMA_NODE   0

static unsigned int         num_ranks        = DEFAULT_NUM_RANKS;
static char               * cpu_list         = DEFAULT_CPU_LIST;
static unsigned char        use_large_pages  = DEFAULT_USE_LARGE_PAGES;
static unsigned char        use_smartmap     = DEFAULT_USE_SMARTMAP;
static unsigned long long   heap_size        = DEFAULT_HEAP_SIZE;
static unsigned long long   stack_size       = DEFAULT_STACK_SIZE;
static char               * name             = NULL;
static char               * envp             = DEFAULT_ENVP;
static char               * exe_path         = NULL;
static char               * exe_argv         = NULL;
static char		  * hio_exe_path     = NULL;
static char		  * hio_exe_argv     = DEFAULT_HIO_ARGV;
static char		  * hio_envp         = DEFAULT_HIO_ENVP;
static char		  * hio_enclave	     = DEFAULT_HIO_ENCLAVE;
static unsigned int	    hio_numa	     = DEFAULT_HIO_NUMA_NODE;

static int cmd_line_np          = 0;
static int cmd_line_cpu_list    = 0;
static int cmd_line_large_pages = 0;
static int cmd_line_smartmap    = 0;
static int cmd_line_heap_size   = 0;
static int cmd_line_stack_size  = 0;
static int cmd_line_name        = 0;
static int cmd_line_envp        = 0;
static int cmd_line_exe         = 1;
static int cmd_line_hio		= 0;
static int cmd_line_hio_args	= 0;
static int cmd_line_hio_envp	= 0;
static int cmd_line_hio_enclave	= 0;
static int cmd_line_hio_numa	= 0;


static int terminate = 0;

static void usage() {
    printf("launch_app: App Launch utility for Hobbes\n\n"		                                   \
	   " Launches an application as specified in command line options or in a job_file.\n\n"           \
	   "Usage: launch_app <enclave_name> [options] <-f job_file | exe args...>\n"                      \
	   " Options: \n"						                                   \
	   "\t[-np <ranks>]                  (default: 1)        : Number of ranks  \n"                     \
	   "\t[--cpulist=<cpus>]             (default: 0,1,2...) : comma separated list of target CPUs \n"  \
	   "\t[--use_large_pages]            (default: n)        : Use large pages  \n"                     \
	   "\t[--use-smartmap]               (default: n)        : Use smartmap     \n"                     \
	   "\t[--heap_size=<size in MB>]     (default: 16MB)     : Heap size in MB  \n"            	   \
	   "\t[--stack_size=<size in MB>]    (default: 256KB)    : Stack size in MB \n" 		           \
	   "\t[--name=<name>]                (default: exe name) : Name of Job      \n"		           \
	   "\t[--envp=<envp>]                (default: NULL)     : ENVP string      \n"		           \
	   "\t[--with-hio=<stub exe>]        (default: NULL)     : Launch HIO stub for the app\n"	   \
	   "\t[--with-hio-args=<args>]       (default: NULL)     : Argument string for HIO stub\n"	   \
	   "\t[--with-hio-envp=<envp>]	     (default: NULL)	 : ENVP string for HIO stub\n"		   \
	   "\t[--with-hio-enclave=<enclave>] (default: master)   : Enclave to launch HIO stub in\n"	   \
	   "\t[--with-hio-numa=<numa node>]  (default: 0)        : NUMA node for HIO memory allocation\n"  \
	   );
    
    exit(-1);
}


static int __app_stub(hobbes_id_t enclave_id);


int launch_app_main(int argc, char ** argv) {
    int         use_job_file   = 0;
    hobbes_id_t enclave_id     = HOBBES_INVALID_ID;

    /* Parse Options */
    {
	int  opt_index = 0;
	char c         = 0;
	
	opterr = 1;

	static struct option long_options[] = {
	    {"np",               required_argument, &cmd_line_np,          1},
	    {"cpulist",          required_argument, &cmd_line_cpu_list,    1},
	    {"use-large-pages",  no_argument,       &cmd_line_large_pages, 1},
	    {"use-smartmap",     no_argument,       &cmd_line_smartmap,    1},
	    {"heap_size",        required_argument, &cmd_line_heap_size,   1},
	    {"stack_size",       required_argument, &cmd_line_stack_size,  1},
	    {"name",             required_argument, &cmd_line_name,        1},
	    {"envp",             required_argument, &cmd_line_envp,        1},
	    {"with-hio",	 required_argument, &cmd_line_hio,	   1},
	    {"with-hio-args",	 required_argument, &cmd_line_hio_args,	   1},
	    {"with-hio-envp",	 required_argument, &cmd_line_hio_envp,	   1},
	    {"with-hio-enclave", required_argument, &cmd_line_hio_enclave, 1},
	    {"with-hio-numa",    required_argument, &cmd_line_hio_numa,	   1},
	    {0, 0, 0, 0}
	};


	while ((c = getopt_long_only(argc, argv, "f:", long_options, &opt_index)) != -1) {
	    switch (c) {
		case 'f': 
		    printf("JOBFILE\n");
		    use_job_file = 1;
		    cmd_line_exe = 0;
		    break;
		case 0:
		    

		    switch (opt_index) {
			case 0: {

			    if (!isdigit(*optarg)) {
				usage();
			    }
			 
			    num_ranks = strtol(optarg, NULL, 10);
			    
			    if (num_ranks == 0) {
				// Invalid Setting _OR_ error from a corrupt string
				usage();
			    }
			    break;
			}
			case 1: {
			    cpu_list = optarg;
			    break;
			}
			case 2: {
			    use_large_pages = 1;
			    break;
			}
			case 3: {
			    use_smartmap = 1;
			    break;
			}
			case 4: {
			    unsigned long long heap_size_in_MB = 0;

			    if (!isdigit(*optarg)) {
				usage();
			    }
			 
			    heap_size_in_MB = strtol(optarg, NULL, 10);
			    
			    if (heap_size_in_MB == 0) {
				// Invalid Setting _OR_ error from a corrupt string
				usage();
			    }

			    heap_size = heap_size_in_MB * 1024 * 1024;
		    
			    break;
			}
			case 5: {
			    unsigned long long stack_size_in_MB = 0;

			    if (!isdigit(*optarg)) {
				usage();
			    }
			 
			    stack_size_in_MB = strtol(optarg, NULL, 10);
			    
			    if (stack_size_in_MB == 0) {
				// Invalid Setting _OR_ error from a corrupt string
				usage();
			    }

			    stack_size = stack_size_in_MB * 1024 * 1024;
		    
			    break;
			}
			case 6: {
			    name = optarg;
			    break;
			} 
			case 7: {
			    envp = optarg;
			    break;
			}
			case 8: {
			    hio_exe_path = optarg;
			    break;
			}
			case 9: {
			    hio_exe_argv = optarg;
			    break;
			}
			case 10: {
			    hio_envp = optarg;
			    break;
			}
			case 11: {
			    hio_enclave = optarg;
			    break;
			}
			case 12: {
			    uint32_t numa = HOBBES_INVALID_NUMA_ID;

			    numa = smart_atou32(numa, optarg);
			    if (numa == HOBBES_INVALID_NUMA_ID) {
				ERROR("Invalid NUMA node specified\n");
				usage();
			    }

			    hio_numa = numa;
			    break;
			}
			default:
			    break;

		    };
		    
		    break;
		case '?':
		default:
		    printf("Error parsing command line (%c)\n", c);
		    usage();
	    }

	}
	

	/*  At this point we have <enclave> <exe_path> <exe_args> left 
	 *  OR if a job file is set then just <enclave> left 
	 */

	

	if (use_job_file) {
	    printf("Error Job files not supported yet\n");
	    exit(0);
	    
	    if (optind + 1 != argc) {
		usage();
	    }
	    
	    enclave_id = hobbes_get_enclave_id(argv[optind]);
	    
	} else {
	    int i = 0;


	    if (optind + 2 > argc) {
		usage();
	    }

	    printf("enclave = %s\n", argv[optind]);

	    enclave_id = hobbes_get_enclave_id(argv[optind]);
	    exe_path   = argv[optind + 1];
	    
	    for (i = 0; i < argc - (optind + 2); i++) {

		if (exe_argv) {
		    char * tmp_argv = NULL;

		    asprintf(&tmp_argv, "%s %s", exe_argv, argv[optind + i + 2]);

		    free(exe_argv);
		
		    exe_argv = tmp_argv;
		} else {
		    exe_argv = argv[optind + i + 2];
		}
	    }
	    	    
	}

    }


    if (name == NULL) {
	name = strrchr(exe_path, '/');

	if (name == NULL) {
	    name = exe_path;
	} else {
	    /* Skip past the '/' */
	    name++;
	}	
    }

    /*
    if (cpu_list == NULL) {
	cpu_mask = 0xffffffffffffffffULL;
    } else {
	char * iter_str = NULL;
	
	while ((iter_str = strsep(&cpu_list, ","))) {

	    int idx = atoi(iter_str);
	    
	    if ((idx == 0) && (iter_str[0] != '0')) {
		printf("Error: Invalid CPU entry (%s)\n", iter_str);
		return -1;
	    }

	    cpu_mask |= (0x1ULL << idx);
	}
    }
    */
 
    /* Launch app */
    return __app_stub(enclave_id);
}


static void
sigint_handler(int signum)
{
    terminate = 1;
}


static int
__app_exited(hobbes_id_t app_id)
{

    app_state_t state;

    if (app_id == HOBBES_INVALID_ID)
	return 0;

    state = hobbes_get_app_state(app_id);
    switch(state) {
	case APP_STOPPED:
	case APP_CRASHED:
	case APP_ERROR:
	    return 1;

	default:
	    return 0;
    }
}


static void
__kill_app(hobbes_id_t enclave_id,
	   hobbes_id_t app_id)
{
    app_state_t state;

    if (app_id == HOBBES_INVALID_ID)
	return;

    state = hobbes_get_app_state(app_id);
    if (state != APP_RUNNING)
	return;

    hobbes_kill_app(enclave_id, app_id);
}


/* Launch app */
static int
__app_stub(hobbes_id_t enclave_id)
{
    hobbes_id_t       hio_enclave_id   = HOBBES_INVALID_ID;
    hobbes_id_t       app_id           = HOBBES_INVALID_ID;
    hobbes_id_t       hio_app_id       = HOBBES_INVALID_ID;
    hobbes_app_spec_t app_spec         = NULL;
    hobbes_app_spec_t hio_app_spec     = NULL;
    enclave_type_t    enclave_type     = INVALID_ENCLAVE;
    hnotif_t          notifier         = NULL; 
    int               ret              = -1;
    uint8_t           use_prealloc_mem = 0;
    uintptr_t         data_pa          = HOBBES_INVALID_ADDR;
    uintptr_t         heap_pa          = HOBBES_INVALID_ADDR;
    uintptr_t         stack_pa         = HOBBES_INVALID_ADDR;
    uint64_t          data_size        = 0;
    uint64_t          page_size        = 0;

    if (use_large_pages)
	page_size = PAGE_SIZE_2MB;
    else
	page_size = PAGE_SIZE_4KB;

    /* Align sizes */
    heap_size  = PAGE_ALIGN_UP(heap_size, page_size);
    stack_size = PAGE_ALIGN_UP(stack_size, page_size);
 
    enclave_type = hobbes_get_enclave_type(enclave_id);
    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Invalid enclave type: cannot launch app\n");
	return -1;
    }

    /* Create the notifier */
    notifier = hnotif_create(HNOTIF_EVT_APPLICATION);
    if (notifier == NULL) {
	ERROR("Could not create hobbes notifier: cannot launch app\n");
	return -1;
    }

    /* Create HIO app */
    if (hio_exe_path != NULL) {
	char stub_name[64] = {0};
	snprintf(stub_name, 64, "%s-hio", name);

	/* Ensure the target enclave is Pisces */
	if (enclave_type != PISCES_ENCLAVE) {
	    ERROR("Cannot launch HIO-enabled application in enclave type: %s\n", enclave_type_to_str(enclave_type));
	    goto hio_out;
	}

	hio_enclave_id = hobbes_get_enclave_id(hio_enclave);
	if (hio_enclave_id == HOBBES_INVALID_ID) {
	    ERROR("Cannot launch HIO stub in enclave %s: no such enclave\n",
		hio_enclave);
	    goto hio_out;
	}

	hio_app_id = hobbes_create_app(stub_name, hio_enclave_id, HOBBES_INVALID_ID);
	if (hio_app_id == HOBBES_INVALID_ID) {
	    ERROR("Could not create HIO app\n");
	    goto hio_out;
	}
	
	hio_app_spec = hobbes_init_hio_app(
		    hio_app_id,
		    stub_name,
		    exe_path,
		    hio_exe_path,
		    hio_exe_argv,
		    hio_envp,
		    page_size,
		    hio_numa,
		    heap_size,
		    stack_size,
		    &data_size,
		    &data_pa,
		    &heap_pa,
		    &stack_pa
	    );

	if (hio_app_spec == NULL) {
	    ERROR("Error initializing HIO app\n");
	    goto hio_init_out;
	}

	/* Now, assign each region as allocated memory to the host enclave */
	ret = hobbes_assign_memory(enclave_id, data_pa, data_size, true, false);
	if (ret != 0) {
	    ERROR("Could not assign data region to host enclave\n");
	    goto assign_data_out;
	}

	ret = hobbes_assign_memory(enclave_id, heap_pa, heap_size, true, false);
	if (ret != 0) {
	    ERROR("Could not assign heap region to host enclave\n");
	    goto assign_heap_out;
	}

	ret = hobbes_assign_memory(enclave_id, stack_pa, stack_size, true, false);
	if (ret != 0) {
	    ERROR("Could not assign stack region to host enclave\n");
	    goto assign_stack_out;
	}

	use_prealloc_mem = 1;
    }

    /* Create app */
    {

	app_id = hobbes_create_app(name, enclave_id, hio_app_id);
	if (app_id == HOBBES_INVALID_ID) {
	    ERROR("Could not create app\n");
	    goto create_out;
	}

	app_spec = hobbes_build_app_spec(
			    app_id,
			    name, 
			    exe_path, 
			    exe_argv, 
			    envp, 
			    cpu_list,
			    use_large_pages, 
			    use_smartmap, 
			    num_ranks, 
			    heap_size, 
			    stack_size,
			    use_prealloc_mem,
			    data_pa,
			    heap_pa,
			    stack_pa
		    );
	if (app_spec == NULL) {
	    ERROR("Could not build app spec\n");
	    goto spec_out;
	}
    }

    /* Launch HIO app */
    if (hio_app_id != HOBBES_INVALID_ID) {
	ret = hobbes_launch_app(hio_enclave_id, hio_app_spec);
	if (ret != 0) {
	    ERROR("Error launching HIO applicationn");
	    goto hio_launch_out;
	}
    }

    /* Give HIO some time to come up. TODO: figure out a better way to do this */
    sleep(1);

    /* Launch app */
    {
	ret = hobbes_launch_app(enclave_id, app_spec);
	if (ret != 0) {
	    ERROR("Error launching application\n");
	    goto launch_out;
	}
    }

    /* Catch sigint to let user kill the app */
    {
	struct sigaction new_action, old_action;

	new_action.sa_handler = sigint_handler;
	sigemptyset(&(new_action.sa_mask));

	sigaction(SIGINT, NULL, &old_action);
	sigaction(SIGINT, &new_action, NULL);
    }


    /* Wait for events on launch fd */
    while (!terminate) {
	int app_exited = 0;
	int hio_exited = 0;
	int fd         = hnotif_get_fd(notifier);

	fd_set rset;
	FD_ZERO(&rset);
	FD_SET(fd, &rset);

	ret = select(fd + 1, &rset, NULL, NULL, NULL);
	if (ret == -1) {
	    if (errno == EINTR)
		continue;

	    ERROR("Select error\n");
	    terminate = 1;
	}

	assert(FD_ISSET(fd, &rset));
	hnotif_ack(fd);

	app_exited = __app_exited(app_id);
	hio_exited = __app_exited(hio_app_id);

	if (app_exited || hio_exited) {
	    if (app_exited) {
		printf("App exited (state=%s)\n"
			"Tearing down HIO stub and exiting\n",
			app_state_to_str(hobbes_get_app_state(app_id)));
	    } else {
		printf("HIO stub exited (state=%s)\n"
			"Tearing down app and exiting because the HIO behavior is now undefined\n",
			app_state_to_str(hobbes_get_app_state(hio_app_id)));
	    }

	    terminate = 1;
	}
    }

    /* Kill app */
    __kill_app(enclave_id, app_id);

launch_out:
    /* Kill stub */
    __kill_app(hio_enclave_id, hio_app_id);

hio_launch_out:
    hobbes_free_app_spec(app_spec);

spec_out:
    hobbes_free_app(app_id);

create_out:
    if (hio_app_id != HOBBES_INVALID_ID)
	hobbes_remove_memory(enclave_id, stack_pa, stack_size, 1);

assign_stack_out:
    if (hio_app_id != HOBBES_INVALID_ID)
	hobbes_remove_memory(enclave_id, heap_pa, heap_size, 1);

assign_heap_out:
    if (hio_app_id != HOBBES_INVALID_ID)
	hobbes_remove_memory(enclave_id, data_pa, data_size, 1);

assign_data_out:
    if (hio_app_id != HOBBES_INVALID_ID) {
	hobbes_free_app_spec(hio_app_spec);
	hobbes_deinit_hio_app();
    }

hio_init_out:
    if (hio_app_id != HOBBES_INVALID_ID)
	hobbes_free_app(hio_app_id);

hio_out:
    hnotif_free(notifier);
    return ret;
}
