/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>


#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes.h"
#include "hobbes_util.h"
#include "hobbes_enclave.h"
#include "pisces_enclave_ctrl.h"
#include "hobbes_db.h"

extern hdb_db_t hobbes_master_db;






/*
static int
read_file(int             fd, 
	  int             size, 
	  unsigned char * buf) 
{
    int left_to_read = size;
    int have_read    = 0;

    while (left_to_read != 0) {
	int bytes_read = read(fd, buf + have_read, left_to_read);

	if (bytes_read <= 0) {
	    break;
	}

	have_read    += bytes_read;
	left_to_read -= bytes_read;
    }

    if (left_to_read != 0) {
	printf("Error could not finish reading file\n");
	return -1;
    }
    
    return 0;
}
*/

#include "enclave_linux_vm.h"


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


    if (strncmp("enclave", pet_xml_tag_name(xml), strlen("enclave")) != 0) {
	ERROR("Invalid XML Config: Not an enclave config\n");
	return -1;
    }

    type = pet_xml_get_val(xml, "type");
    
    if (type == NULL) {
	ERROR("Enclave type not specified\n");
	return -1;
    }

    
    if (strncasecmp(type, "pisces", strlen("pisces")) == 0) {

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
   

    if (enclave_type == PISCES_ENCLAVE) {
	return pisces_enclave_destroy(enclave_id);
    } else if (enclave_type == LINUX_VM_ENCLAVE) {
	return destroy_linux_vm(enclave_id);

	/*
	  } else if (enclave.type == PISCES_VM_ENCLAVE) { 
	*/
    } else {
	ERROR("Invalid Enclave Type (%d)\n", enclave_type);
	return -1;
    }

    return 0;
    
}


hobbes_id_t
hobbes_get_enclave_id(char * enclave_name)
{	
    return hdb_get_enclave_id(hobbes_master_db, enclave_name);
}

char * 
hobbes_get_enclave_name(hobbes_id_t enclave_id)
{
    return hdb_get_enclave_name(hobbes_master_db, enclave_id);
}

int 
hobbes_set_enclave_state(hobbes_id_t     enclave_id, 
			 enclave_state_t state) 
{
    return hdb_set_enclave_state(hobbes_master_db, enclave_id, state);
}

hobbes_id_t
hobbes_get_enclave_state(hobbes_id_t     enclave_id) 
{
    return hdb_get_enclave_state(hobbes_master_db, enclave_id);
}


hcq_handle_t 
hobbes_open_enclave_cmdq(hobbes_id_t enclave_id)
{
    xemem_segid_t  segid        = -1;
    enclave_type_t enclave_type = INVALID_ENCLAVE;

    enclave_type = hdb_get_enclave_type(hobbes_master_db, enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return HCQ_INVALID_HANDLE;
    }
    
    segid = hdb_get_enclave_cmdq(hobbes_master_db, enclave_id);

    return hcq_connect(segid);
}

void
hobbes_close_enclave_cmdq(hcq_handle_t hcq)
{
    hcq_disconnect(hcq);

}



int 
hobbes_register_enclave_cmdq(hobbes_id_t   enclave_id, 
			     xemem_segid_t segid)
{
    enclave_type_t enclave_type = INVALID_ENCLAVE;

    enclave_type = hdb_get_enclave_type(hobbes_master_db, enclave_id);

    if (enclave_type == INVALID_ENCLAVE) {
	ERROR("Could not find enclave (%d)\n", enclave_id);
	return -1;
    }
    
    return hdb_set_enclave_cmdq(hobbes_master_db, enclave_id, segid);
}


struct enclave_info * 
hobbes_get_enclave_list(int * num_enclaves)
{
    struct enclave_info * info_arr = NULL;
    hobbes_id_t         * id_arr   = NULL;

    int id_cnt = 0;
    int i      = 0;

    id_arr = hdb_get_enclaves(hobbes_master_db, &id_cnt);
    
    if (id_arr == NULL) {
	ERROR("Could not retrieve list of enclave ids\n");
	return NULL;
    }
	
    info_arr = calloc(sizeof(struct enclave_info), id_cnt);

    for (i = 0; i < id_cnt; i++) {
	info_arr[i].id    = id_arr[i];
	info_arr[i].type  = hdb_get_enclave_type(hobbes_master_db, id_arr[i]);
	info_arr[i].state = hdb_get_enclave_state(hobbes_master_db, id_arr[i]);

	strncpy(info_arr[i].name, hdb_get_enclave_name(hobbes_master_db, id_arr[i]), 31);
    }

    free(id_arr);

    *num_enclaves = id_cnt;

    return info_arr;
}



const char * 
enclave_type_to_str(enclave_type_t type) 
{
    switch (type) {
	case INVALID_ENCLAVE:   return "INVALID_ENCLAVE";
	case MASTER_ENCLAVE:    return "MASTER_ENCLAVE";
	case PISCES_ENCLAVE:    return "PISCES_ENCLAVE";
	case PISCES_VM_ENCLAVE: return "PISCES_VM_ENCLAVE";
	case LINUX_VM_ENCLAVE:  return "LINUX_VM_ENCLAVE";

	default : return NULL;
    }

    return NULL;
}

const char *
enclave_state_to_str(enclave_state_t state) 
{
    switch (state) {
	case ENCLAVE_INITTED: return "Initialized";
	case ENCLAVE_RUNNING: return "Running";
	case ENCLAVE_STOPPED: return "Stopped";
	case ENCLAVE_CRASHED: return "Crashed";
	case ENCLAVE_ERROR:   return "Error";

	default: return NULL;
    }

    return NULL;
}
    
