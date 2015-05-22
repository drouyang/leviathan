#include <hobbes_db.h>

#include <pet_xml.h>
#include <pet_log.h>

#include "enclave.h"
#include "pisces_enclave_ctrl.h"
#include "linux_vm_ctrl.h"
#include "pisces_vm_ctrl.h"

extern hdb_db_t hobbes_master_db;

int 
hobbes_create_enclave(char * cfg_file_name, 
		      char * name)
{
    pet_xml_t   xml  = NULL;
    char      * type = NULL; 

    xml = pet_xml_open_file(cfg_file_name);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file_name);
	return -1;
    }

    type =  pet_xml_tag_name(xml);

    if (strncmp("pisces", type, strlen("pisces")) == 0) {

	DEBUG("Creating Pisces Enclave\n");
	return pisces_enclave_create(xml, name);

    } else if (strncasecmp(type, "vm", strlen("vm")) == 0) {
	char * target = pet_xml_get_val(xml, "host_enclave");
	
	if (!target) {
	    DEBUG("Creating Palacios/Linux Enclave\n");
	    
	    // run locally
	    create_linux_vm(xml, name);
	} else {
	    DEBUG("Creating Palacios/Pisces Enclave\n");
	    // run on pisces enclave

	    create_pisces_vm(xml, name);
	}

    } else {
	ERROR("Invalid Enclave Type (%s)\n", type);
	return -1;
    }



    return 0;

}



int 
hobbes_destroy_enclave(hobbes_id_t enclave_id)
{
    enclave_type_t enclave_type = INVALID_ENCLAVE;

    enclave_type = hdb_get_enclave_type(hobbes_master_db, enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }
   
    switch (enclave_type) {
	case PISCES_ENCLAVE:
	    return pisces_enclave_destroy(enclave_id);
	case LINUX_VM_ENCLAVE:
	    return destroy_linux_vm(enclave_id);
	case PISCES_VM_ENCLAVE:
	    return destroy_pisces_vm(enclave_id);
	default:
	    ERROR("Invalid Enclave Type (%d)\n", enclave_type);
	    return -1;
    }

    return 0;
    
}



int 
create_enclave_main(int argc, char ** argv)
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

    return hobbes_create_enclave(cfg_file, name);
}


int 
destroy_enclave_main(int argc, char ** argv)
{
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;

    if (argc < 1) {
	printf("Usage: hobbes destroy_enclave <enclave name>\n");
	return -1;
    }
    
    enclave_id = hobbes_get_enclave_id(argv[1]);
    

    return hobbes_destroy_enclave(enclave_id);
}


int
ping_enclave_main(int argc, char ** argv)
{
    hobbes_id_t enclave_id = HOBBES_INVALID_ID;
    
    if (argc < 1) {
	printf("Usage: hobbes ping_enclave <enclave name>\n");
	return -1;
    }
	   
    enclave_id = hobbes_get_enclave_id(argv[1]);

    return hobbes_ping_enclave(enclave_id);
}

int
list_enclaves_main(int argc, char ** argv)
{
    struct enclave_info * enclaves = NULL;
    int num_enclaves = -1;
    int i = 0;

    enclaves = hobbes_get_enclave_list(&num_enclaves);

    if (enclaves == NULL) {
	ERROR("Could not retrieve enclave list\n");
	return -1;
    }
	
    printf("%d Active Enclaves:\n", num_enclaves);
    printf("--------------------------------------------------------------------------------\n");
    printf("| ID       | Enclave name                     | Type             | State       |\n");
    printf("--------------------------------------------------------------------------------\n");

 
    for (i = 0; i < num_enclaves; i++) {
	printf("| %-*d | %-*s | %-*s | %-*s |\n", 
	       8, enclaves[i].id,
	       32, enclaves[i].name,
	       16, enclave_type_to_str(enclaves[i].type), 
	       11, enclave_state_to_str(enclaves[i].state));
    }

    printf("--------------------------------------------------------------------------------\n");

    free(enclaves);

    return 0;
}
