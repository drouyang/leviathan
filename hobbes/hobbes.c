/* Hobbes Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xpmem.h>
#include <pet_log.h>

#include "enclave.h"
#include "client.h"

const char * hobbes_prog_version = "Hobbes 0.1";
const char * bug_email_addr      = "<jacklange@cs.pitt.edu>";



struct args 
{


};



static int
create_enclave_handler(int argc, char ** argv) 
{
    char * cfg_file = NULL;
    char * name     = NULL;

    if (argc < 1) {
	printf("Usage: hobbes create_enclave <cfg_file> [name] [-t <host_enclave>]\n");
	return -1;
    }

    cfg_file = argv[1];
    
    if (argc >= 2) {
	name = argv[2];
    }

    return create_enclave(cfg_file, name);
}


static int 
destroy_enclave_handler(int argc, char ** argv)
{
    if (argc < 1) {
	printf("Usage: hobbes destroy_enclave <enclave name>\n");
	return -1;
    }

    return destroy_enclave(argv[1]);
}

static int
launch_job_handler(int argc, char ** argv)
{
    return -1;
}

static int
list_enclaves_handler(int argc, char ** argv)
{
    struct hobbes_enclave * list = NULL;
    int num_enclaves = -1;
    int i = 0;

    list = hdb_get_enclave_list(hobbes_master_db, &num_enclaves);

    if (!list) {
	ERROR("Could not retrieve enclave list\n");
	return -1;
    }
	
    printf("%d Active Enclaves:\n", num_enclaves);
 
    for (i = 0; i < num_enclaves; i++) {
	printf("%lu: %-35s [%s] <%s>\n", list[i].enclave_id,
	       list[i].name,
	       enclave_type_to_str(list[i].type), 
	       enclave_state_to_str(list[i].state));

    }


    hdb_free_enclave_list(list);

    return 0;
}

static int
list_segments_handler(int argc, char ** argv)
{
    struct hobbes_segment * list = NULL;
    int num_segments = 0, i;

    if (argc != 1) {
        printf("Usage: hobbes list_segments\n");
        return -1;
    }

    list = hdb_get_segment_list(hobbes_master_db, &num_segments);

    if (list == NULL) {
        ERROR("Could not retrieve segment list\n");
        return -1;
    }

    printf("%d segments:\n", num_segments);

    for (i = 0; i < num_segments; i++) {
        printf("%s: %lli\n",
            list[i].name,
            list[i].segid);
    }

    hdb_free_segment_list(list);

    return 0;
}

static int
remove_segment_handler(int argc, char ** argv)
{
    char *name;

    if (argc != 2) {
        printf("Usage: hobbes remove_segment <name>\n");
        return -1;
    }

    name = *(++argv);

    return hdb_remove_segment(hobbes_master_db, 0, name);
}


struct hobbes_cmd {
    char * name;
    int (*handler)(int argc, char ** argv);   
    char * desc;
};

static struct hobbes_cmd cmds[] = {
    {"create_enclave",  create_enclave_handler,  "Create Native Enclave"},
    {"destroy_enclave", destroy_enclave_handler, "Destroy Native Enclave"},
    {"list_enclaves",   list_enclaves_handler,   "List all running enclaves"},
    {"launch_job",      launch_job_handler,      "Launch a job in an enclave"},
    {"list_segments",   list_segments_handler,   "List all exported xpmem segments"},
    {"remove_segment",  remove_segment_handler,  "Remove an exported xpmem segment from the DB"},
    {0, 0, 0}
};



static void 
usage() 
{
    int i = 0;

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

    hobbes_client_init();

    while (cmds[i].name) {

	if (strncmp(cmds[i].name, argv[1], strlen(cmds[i].name)) == 0) {
	    return cmds[i].handler(argc - 1, &argv[1]);
	}

	i++;
    }
    
    hobbes_client_deinit();


    return 0;
}
