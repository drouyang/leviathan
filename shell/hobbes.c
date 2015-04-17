/* Hobbes Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pet_log.h>

#include <hobbes.h>
#include <xemem.h>

static const char * hobbes_prog_version = "Hobbes Runtime Shell 0.1";
static const char * bug_email_addr      = "<jacklange@cs.pitt.edu>";


static int
list_segments_handler(int argc, char ** argv)
{
    struct xemem_segment * seg_arr = NULL;
    int num_segments = 0;
    int i = 0;

    if (argc != 1) {
        printf("Usage: hobbes list_segments\n");
        return -1;
    }

    seg_arr = xemem_get_segment_list(&num_segments);

    if (seg_arr == NULL) {
        ERROR("Could not retrieve XEMEM segment list\n");
        return -1;
    }

    printf("%d segments:\n", num_segments);

    for (i = 0; i < num_segments; i++) {
        printf("%s: %lu\n",
            seg_arr[i].name,
            seg_arr[i].segid);
    }

    free(seg_arr);

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
extern int   list_enclaves_main(int argc, char ** argv);

static struct hobbes_cmd cmds[] = {
    {"create_enclave",  create_enclave_main,     "Create Native Enclave"},
    {"destroy_enclave", destroy_enclave_main,    "Destroy Native Enclave"},
    {"list_enclaves",   list_enclaves_main,      "List all running enclaves"},
    {"list_segments",   list_segments_handler,   "List all exported xpmem segments"},
    {"launch_app",      launch_app_main,         "Launch an application in an enclave"},
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
