/* Pisces VM control 
 *  (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <getopt.h>

#include <pet_log.h>


#include <hobbes.h>
#include <hobbes_enclave.h>
#include <hobbes_util.h>


#include "vm.h"

/* Two options here:
 *  (1) We launch the VM into a currently running enclave, 
 *  OR 
 *  (2) We create a new dedicated enclave to host the VM
 */ 



static void create_vm_usage() {
    printf("create_vm: Hobbes VM launch\n\n"				                \
           " Launches a VM as specified in command line options or in a job_file.\n\n"  \
           "Usage: create_vm <-f job_file | vm cfg args...> [options]\n"                \
           " Options: \n"						                \
           "\t-n, --name=<name>             : Name of VM                \n"		\
           "\t-t, --host=<enclave_name>     : Name of host enclave      \n"             \
	   "\t-h, --help                    : Display help dialog       \n"             \
           );

}



int 
create_vm_main(int argc, char ** argv)
{
    hobbes_id_t host_enclave_id = HOBBES_INVALID_ID;

    char      * cfg_file = NULL;
    char      * name     = NULL;
    char      * host     = NULL;

    /* Parse Options */
    {
        int  opt_index = 0;
        char c         = 0;

        opterr = 1;

        static struct option long_options[] = {
            {"name",     required_argument, 0,  'n'},
	    {"host",     required_argument, 0,  't'},
	    {"help",     no_argument,       0,  'h'},
	    {0, 0, 0, 0}
        };


        while ((c = getopt_long(argc, argv, "f:n:t:h", long_options, &opt_index)) != -1) {
            switch (c) {
                case 'f': 
		    cfg_file = optarg;
                    break;
		case 'n':
		    name = optarg;
		    break;
		case 't':
		    host = optarg;
		    break;
		case 'h':
		default:
		    create_vm_usage();
		    return -1;
	    }
	}
    }

    if (cfg_file == NULL) {
	create_vm_usage();
	return -1;
    }

    if (host) {
	host_enclave_id = hobbes_get_enclave_id(host);

	if (host_enclave_id == HOBBES_INVALID_ID) {
	    ERROR("Invalid Host enclave (%s)\n", host);
	    return -1;
	}
    }

    return hobbes_create_vm(cfg_file, name, host_enclave_id);
}



static int 
__create_vm(pet_xml_t   xml,
	    char      * name,
	    hobbes_id_t host_enclave_id)
{
    char   * target  = pet_xml_get_val(xml, "host_enclave");
    char   * err_str = NULL;
    uint32_t err_len = 0;
    int      ret     = -1;

    if (host_enclave_id == HOBBES_INVALID_ID) {

	host_enclave_id = hobbes_get_enclave_id(target);
	
	if (host_enclave_id == HOBBES_INVALID_ID) {
	    ERROR("Could not find target Enclave (%s) for VM, launching on master enclave\n", target);
	    host_enclave_id = HOBBES_MASTER_ENCLAVE_ID;
	}
    }
       
    /* Copy aux files over */
    {
	hcq_handle_t hcq = hobbes_open_enclave_cmdq(host_enclave_id);
	hcq_cmd_t    cmd = HCQ_INVALID_CMD;
	char       * str = NULL;
	
	if (hcq == HCQ_INVALID_HANDLE) {
	    ERROR("Could not open command queue to enclave %d (%s)\n", 
		  host_enclave_id, hobbes_get_enclave_name(host_enclave_id));
	    return -1;
	}

	str = pet_xml_get_str(xml);

	if (str == NULL) {
	    ERROR("Could not convert VM XML spec to string\n");	    
	    hobbes_close_enclave_cmdq(hcq);
	    return -1;
	}

	cmd = hcq_cmd_issue(hcq, HOBBES_CMD_VM_LAUNCH, strlen(str) + 1, str);

	free(str);

	if (cmd == HCQ_INVALID_CMD) {
	    ERROR("Could not issue command to command queue\n");
	    hobbes_close_enclave_cmdq(hcq);
	    return -1;
	}

	ret     = hcq_get_ret_code(hcq, cmd);	
	err_str = hcq_get_ret_data(hcq, cmd, &err_len);

	if (err_len > 0) {
	    printf("%s\n", err_str);
	}

	hcq_cmd_complete(hcq, cmd);
	hobbes_close_enclave_cmdq(hcq);
    }
    
    return ret;

}

int
hobbes_destroy_vm(hobbes_id_t enclave_id)
{

    hobbes_id_t    host_enclave_id = HOBBES_INVALID_ID;
    enclave_type_t enclave_type    = INVALID_ENCLAVE;
    hcq_handle_t   hcq             = NULL;
    hcq_cmd_t      cmd             = HCQ_INVALID_CMD;

    char   * err_str =  NULL;
    uint32_t err_len =  0;
    int      ret     = -1;


   if (enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }

    enclave_type = hobbes_get_enclave_type(enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }
   
    if ( (enclave_type != PISCES_VM_ENCLAVE) && 
	 (enclave_type != LINUX_VM_ENCLAVE)  ) {
	ERROR("Enclave (%d) is not a VM enclave\n", enclave_id);
	return -1;
    }


    host_enclave_id = hobbes_get_enclave_parent(enclave_id);
    
    if (host_enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find parent enclave for enclave %d (%s)\n", 
	      enclave_id, hobbes_get_enclave_name(enclave_id));
	return -1;
    }

    hcq = hobbes_open_enclave_cmdq(host_enclave_id);
    
    if (hcq == HCQ_INVALID_HANDLE) {
	ERROR("Could not open command queue for enclave %d (%s)\n", 
	      host_enclave_id, hobbes_get_enclave_name(host_enclave_id));
	return -1;
    }

    cmd = hcq_cmd_issue(hcq, HOBBES_CMD_VM_DESTROY, sizeof(hobbes_id_t), &enclave_id);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Could not issue command to command queue\n");
	hobbes_close_enclave_cmdq(hcq);
	return -1;
    }

    ret     = hcq_get_ret_code(hcq, cmd);	
    err_str = hcq_get_ret_data(hcq, cmd, &err_len);


    if (err_len > 0) {
	printf("%s\n", err_str);
    }

    hcq_cmd_complete(hcq, cmd);
    hobbes_close_enclave_cmdq(hcq);

    return ret;
}



int 
hobbes_create_vm(char        * cfg_file, 
		 char        * name,
		 hobbes_id_t   host_enclave_id)
{
    pet_xml_t   xml  = NULL;
    char      * type = NULL; 

    xml = pet_xml_open_file(cfg_file);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file);
	return -1;
    }

    type =  pet_xml_tag_name(xml);

    if (strncmp("vm", type, strlen("vm")) != 0) {
	ERROR("Invalid Enclave Type (%s)\n", type);
	return -1;
    }

    /* Allocate memory */

    /* Update configuration to enable hobbes features */


    DEBUG("Creating VM Enclave\n");
    return __create_vm(xml, name, host_enclave_id);


}





int 
destroy_vm_main(int argc, char ** argv)
{
    hobbes_id_t    enclave_id   = HOBBES_INVALID_ID;

    if (argc < 1) {
	printf("Usage: hobbes destroy_vm <enclave name>\n");
	return -1;
    }
    
    enclave_id = hobbes_get_enclave_id(argv[1]);

    return hobbes_destroy_vm(enclave_id);
}
