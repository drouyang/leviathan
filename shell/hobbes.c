/* Hobbes Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pet_log.h>

#include <hobbes.h>
#include <hobbes_app.h>
#include <hobbes_enclave.h>
#include <xemem.h>

static const char * hobbes_prog_version = "Hobbes Runtime Shell 0.1";
static const char * bug_email_addr      = "<jacklange@cs.pitt.edu>";


static int
list_segments_handler(int argc, char ** argv)
{
    struct xemem_segment * seg_arr = NULL;
    int num_segments = 0;
    int i = 0;

    seg_arr = xemem_get_segment_list(&num_segments);

    if (num_segments == -1) {
	ERROR("could not retrieve segment list\n");
	return -1;
    }

    printf("%d Segments\n", num_segments);

    if (num_segments == 0) {
	return 0;
    }

    printf("-------------------------------------------------------------------------------\n");
    printf("| SEGID          | Segment Name                     | Enclave ID | App ID     |\n");
    printf("-------------------------------------------------------------------------------\n");

    for (i = 0; i < num_segments; i++) {
        printf("| %-*lu | %-*s | %-*d | %-*d |\n",
	       14, seg_arr[i].segid,
	       32, seg_arr[i].name,
	       10, seg_arr[i].enclave_id,
	       10, seg_arr[i].app_id);
    }

    printf("-------------------------------------------------------------------------------\n");

    free(seg_arr);

    return 0;
}


static int
list_apps_main(int argc, char ** argv)
{
    struct app_info * apps = NULL;
    int num_apps = -1;
    int i = 0;

    apps = hobbes_get_app_list(&num_apps);
    
    if (num_apps == -1) {
	ERROR("could not retrieve app list\n");
	return -1;
    }

    printf("%d Applications\n", num_apps);

    if (num_apps == 0) {
	return 0;
    }

    printf("------------------------------------------------------------------------------------------------\n");
    printf("| HPID     | Enclave                          | Application                      | State       |\n");
    printf("------------------------------------------------------------------------------------------------\n");

    for (i = 0; i < num_apps; i++) {
	printf("| %-*d | %-*s | %-*s | %-*s |\n",
	       8,  apps[i].id,
	       32, hobbes_get_enclave_name(apps[i].enclave_id),
	       32, apps[i].name,
	       11, app_state_to_str(apps[i].state));
    }
    
    printf("------------------------------------------------------------------------------------------------\n");

    free(apps);

    return 0;
}


struct hobbes_cmd {
    char * name;
    int (*handler)(int argc, char ** argv);   
    char * desc;
};

extern int      launch_app_main(int argc, char ** argv);
extern int  create_enclave_main(int argc, char ** argv);
extern int destroy_enclave_main(int argc, char ** argv);
extern int       create_vm_main(int argc, char ** argv);
extern int      destroy_vm_main(int argc, char ** argv);
extern int    ping_enclave_main(int argc, char ** argv);
extern int   list_enclaves_main(int argc, char ** argv);
extern int  dump_cmd_queue_main(int argc, char ** argv);
extern int        cat_file_main(int argc, char ** argv);
extern int   cat_into_file_main(int argc, char ** argv);
extern int     list_memory_main(int argc, char ** argv);
extern int       list_cpus_main(int argc, char ** argv);
extern int   assign_memory_main(int argc, char ** argv);
extern int     assign_cpus_main(int argc, char ** argv);
extern int	   console_main(int argc, char ** argv);


static struct hobbes_cmd cmds[] = {
    {"create_enclave"  , create_enclave_main   , "Create Native Enclave"                       },
    {"destroy_enclave" , destroy_enclave_main  , "Destroy Native Enclave"                      },
    {"create_vm"       , create_vm_main        , "Create VM Enclave"                           },
    {"destroy_vm"      , destroy_vm_main       , "Destroy VM Enclave"                          },
    {"ping_enclave"    , ping_enclave_main     , "Ping an enclave"                             },
    {"list_enclaves"   , list_enclaves_main    , "List all running enclaves"                   },
    {"list_segments"   , list_segments_handler , "List all exported xemem segments"            },
    {"launch_app"      , launch_app_main       , "Launch an application in an enclave"         },
    {"list_apps"       , list_apps_main        , "List all applications"                       },
    {"dump_cmd_queue"  , dump_cmd_queue_main   , "Dump the command queue state for an enclave" },
    {"cat_file"        , cat_file_main         , "'cat' a file on an arbitrary enclave"        },
    {"cat_into_file"   , cat_into_file_main    , "'cat' to a file on an arbitrary enclave"     },
    {"list_memory"     , list_memory_main      , "List the status of system memory"            },
    {"list_cpus"       , list_cpus_main        , "List the stuats of local CPUs"               },
    {"assign_memory"   , assign_memory_main    , "Assign memory to an Enclave"		       },
    {"assign_cpus"     , assign_cpus_main      , "Assign cpus to an Enclave"		       },
    {"console"	       , console_main	       , "Attach to an Enclave Console"		       },
    {0, 0, 0}
};



static void 
usage() 
{
    int i = 0;

    printf("%s\n", hobbes_prog_version);
    printf("Report Bugs to %s\n", bug_email_addr);
    printf("Usage: hobbes <command> [args...]\n");
    printf("Commands:\n");

    while (cmds[i].name) {
	printf("\t%-17s -- %s\n", cmds[i].name, cmds[i].desc);
	i++;
    }

    return;
}


int 
main(int argc, char ** argv) 
{
    int i = 0;

    if (argc < 2) {
	usage();
	exit(-1);
    }

    if (hobbes_client_init() != 0) {
	ERROR("Could not initialize hobbes client\n");
	return -1;
    }

    while (cmds[i].name) {

	if (strncmp(cmds[i].name, argv[1], strlen(cmds[i].name)) == 0) {
	    return cmds[i].handler(argc - 1, &argv[1]);
	}

	i++;
    }
    
    hobbes_client_deinit();


    return 0;
}
