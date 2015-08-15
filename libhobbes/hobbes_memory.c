/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */


#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes_util.h"
#include "hobbes_memory.h"
#include "hobbes_cmd_queue.h"

int 
hobbes_assign_memory(hcq_handle_t hcq,
		     uintptr_t    base_addr, 
		     uint64_t     size,
		     bool         allocated,
		     bool         zeroed)
{

    hcq_cmd_t cmd      = HCQ_INVALID_CMD;
    pet_xml_t cmd_xml  = PET_INVALID_XML;

    uint32_t  ret_size =  0;
    uint8_t * ret_data =  NULL;

    char    * tmp_str  =  NULL;
    int       ret      = -1;

    

    cmd_xml = pet_xml_new_tree("memory");

    if (cmd_xml == PET_INVALID_XML) {
        ERROR("Could not create xml command\n");
        goto err;
    }

    /* Base Address */
    if (asprintf(&tmp_str, "%p", (void *)base_addr) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "base_addr",  tmp_str);
    smart_free(tmp_str);

    /* Size */
    if (asprintf(&tmp_str, "%lu", size) == -1) {
	tmp_str = NULL;
	goto err;
    }

    pet_xml_add_val(cmd_xml, "size",  tmp_str);
    smart_free(tmp_str);
    
    pet_xml_add_val(cmd_xml, "allocated", (allocated) ? "1" : "0");
    pet_xml_add_val(cmd_xml, "zeroed",    (zeroed)    ? "1" : "0");

    tmp_str = pet_xml_get_str(cmd_xml);
    cmd     = hcq_cmd_issue(hcq, HOBBES_CMD_ADD_MEM, strlen(tmp_str) + 1, tmp_str);
    
    if (cmd == HCQ_INVALID_CMD) {
	ERROR("Error issuing add memory command (%s)\n", tmp_str);
	goto err;
    } 


    ret = hcq_get_ret_code(hcq, cmd);
    
    if (ret != 0) {
	ret_data = hcq_get_ret_data(hcq, cmd, &ret_size);
	ERROR("Error adding memory (%s) [ret=%d]\n", ret_data, ret);
	goto err;
    }

    hcq_cmd_complete(hcq, cmd);

    smart_free(tmp_str);
    pet_xml_free(cmd_xml);

    return ret;

 err:
    if (tmp_str != NULL)            smart_free(tmp_str);                 
    if (cmd_xml != PET_INVALID_XML) pet_xml_free(cmd_xml);
    if (cmd     != HCQ_INVALID_CMD) hcq_cmd_complete(hcq, cmd);
    return -1;
}
