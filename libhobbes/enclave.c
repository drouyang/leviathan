/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "client.h"
#include "cmd_queue.h"

#include <pisces.h>
#include <pisces_ctrl.h>

#include <pet_mem.h>
#include <pet_log.h>
#include <ezxml.h>


static int
smart_atoi(int dflt, char * str) 
{
    char * end = NULL;
    int    tmp = 0;
    
    if ((str == NULL) || (*str == '\0')) {
	/*  String was either NULL or empty */
	return dflt;
    }

    tmp = strtol(str, &end, 10);

    if (*end) {
	/* String contained non-numerics */
	return dflt;
    }
    
    return tmp;
}

static ezxml_t 
open_xml_file(char * filename) 
{
    ezxml_t xml_input = ezxml_parse_file(filename);
    
    if (xml_input == NULL) {
	ERROR("Could not open XML input file (%s)\n", filename);
	return NULL;
    } else if (strcmp("", ezxml_error(xml_input)) != 0) {
	ERROR("%s\n", ezxml_error(xml_input));
	return NULL;
    }

    return xml_input;
}



static char * 
get_val(ezxml_t   cfg,
	char    * tag) 
{
    char   * attrib = (char *)ezxml_attr(cfg, tag);
    ezxml_t  txt    = ezxml_child(cfg, tag);
    char   * val    = NULL;

    if ((txt != NULL) && (attrib != NULL)) {
	ERROR("Invalid Cfg file: Duplicate value for %s (attr=%s, txt=%s)\n", 
	       tag, attrib, ezxml_txt(txt));
	return NULL;
    }

    val = (attrib == NULL) ? ezxml_txt(txt) : attrib;

    /* An non-present value actually == "". So we check if the 1st char is '/0' and return NULL */
    if (!*val) return NULL;

    return val;
}


static ezxml_t 
get_subtree(ezxml_t   tree,
	    char    * tag) 
{
    return ezxml_child(tree, tag);
}




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

#include "enclave_pisces.h"
#include "enclave_linux_vm.h"


int 
create_enclave(char * cfg_file_name, 
	       char * name)
{
    ezxml_t   xml  = NULL;
    char    * type = NULL; 

    xml = open_xml_file(cfg_file_name);
    
    if (xml == NULL) {
	ERROR("Error loading Enclave config file (%s)\n", cfg_file_name);
	return -1;
    }


    if (strncmp("enclave", ezxml_name(xml), strlen("enclave")) != 0) {
	ERROR("Invalid XML Config: Not an enclave config\n");
	return -1;
    }

    type = get_val(xml, "type");
    
    if (type == NULL) {
	ERROR("Enclave type not specified\n");
	return -1;
    }

    
    if (strncasecmp(type, "pisces", strlen("pisces")) == 0) {

	DEBUG("Creating Pisces Enclave\n");
	return create_pisces_enclave(xml, name);

    } else if (strncasecmp(type, "vm", strlen("vm")) == 0) {
	char * target = get_val(xml, "host_enclave");
	
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
destroy_enclave(char * enclave_name)
{
    hdb_id_t       enclave_id   = hdb_get_enclave_id(hobbes_master_db, enclave_name);
    enclave_type_t enclave_type = INVALID_ENCLAVE;

    if (enclave_id == -1) {
	ERROR("Could not find enclave (%s)\n", enclave_name);
	return -1;
    }
    
    enclave_type = hdb_get_enclave_type(hobbes_master_db, enclave_id);
   

    if (enclave_type == PISCES_ENCLAVE) {
	return destroy_pisces_enclave(enclave_id);
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


hcq_handle_t 
enclave_open_cmd_queue(char * enclave_name)
{
    hdb_id_t      enclave_id = hdb_get_enclave_id(hobbes_master_db, enclave_name);
    xemem_segid_t segid      = -1;

    if (enclave_id == -1) {
	ERROR("Could not find enclave (%s)\n", enclave_name);
	return HCQ_INVALID_HANDLE;
    }
    
    segid = hdb_get_enclave_cmdq(hobbes_master_db, enclave_id);

    return hcq_connect(segid);
}


int 
enclave_register_cmd_queue(char          * enclave_name, 
			   xemem_segid_t   segid)
{
    hdb_id_t      enclave_id = hdb_get_enclave_id(hobbes_master_db, enclave_name);
    
    if (enclave_id == -1) {
	ERROR("Could not find enclave (%s)\n", enclave_name);
	return -1;
    }
    
    return hdb_set_enclave_cmdq(hobbes_master_db, enclave_id, segid);
}


struct enclave_info * 
get_enclave_list(int * num_enclaves)
{
    struct enclave_info * info_arr = NULL;
    hdb_id_t            * id_arr   = NULL;

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
    
