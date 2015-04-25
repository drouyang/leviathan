/* Pisces VM control 
 *  (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <pet_log.h>


#include <hobbes.h>
#include <hobbes_enclave.h>
#include <hobbes_util.h>


#include "pisces_vm_ctrl.h"
#include "pisces_enclave_ctrl.h"

/* Two options here:
 *  (1) We launch the VM into a currently running enclave, 
 *  OR 
 *  (2) We create a new dedicated enclave to host the VM
 */ 



int 
create_pisces_vm(pet_xml_t   xml,
		 char      * name)
{
    hobbes_id_t host_enclave_id = HOBBES_INVALID_ID;
    char      * target          = pet_xml_get_val(xml, "host_enclave");

    char   * err_str = NULL;
    uint32_t err_len = 0;
    int      ret     = -1;

    host_enclave_id = hobbes_get_enclave_id(target);
    
    if (host_enclave_id == HOBBES_INVALID_ID) {
	ERROR("Could not find target Enclave (%s) for VM\n", target);
	return -1;
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
destroy_pisces_vm(hobbes_id_t enclave_id)
{

    hobbes_id_t  host_enclave_id = HOBBES_INVALID_ID;
    hcq_handle_t hcq             = NULL;
    hcq_cmd_t    cmd             = HCQ_INVALID_CMD;

    char   * err_str =  NULL;
    uint32_t err_len =  0;
    int      ret     = -1;

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

    cmd = hcq_cmd_issue(hcq, HOBBES_CMD_VM_LAUNCH, sizeof(hobbes_id_t), &enclave_id);
    
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
