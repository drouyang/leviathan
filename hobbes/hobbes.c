/* Hobbes Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "enclave.h"

const char * hobbes_prog_version = "Hobbes 0.1";
const char * bug_email_addr      = "<jacklange@cs.pitt.edu>";



struct args 
{


};



static int
create_enclave_handler(int argc, char ** argv) 
{
    char * cfg_file = NULL;

    if (argc < 1) {
	printf("Usage: hobbes create_enclave <cfg_file>\n");
	return -1;
    }

    cfg_file = argv[1];

    return create_enclave(cfg_file);
}


struct hobbes_cmd {
    char * name;
    int (*handler)(int argc, char ** argv);   
    char * desc;
};

static struct hobbes_cmd cmds[] = {
    {"create_enclave", create_enclave_handler, "Create enclave"},
    {0, 0, 0}
};



static void 
usage() 
{
    int i = 0;

    printf("Usage: hobbes <command> [args...]\n");
    printf("Commands:\n");

    while (cmds[i].name) {
	printf("\t%s -- %s\n", cmds[i].name, cmds[i].desc);
	
	i++;
    }

    return;
}


int 
main(int argc, char ** argv) 
{
    int    i   = 0;

    if (argc < 2) {
	usage();
	exit(-1);
    }


    while (cmds[i].name) {

	if (strncmp(cmds[i].name, argv[1], strlen(cmds[i].name)) == 0) {
	    return cmds[i].handler(argc - 1, &argv[1]);
	}

	i++;
    }
    

    return 0;
}
