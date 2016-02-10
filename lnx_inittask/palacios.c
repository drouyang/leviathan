#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>


#include <hobbes.h>
#include <hobbes_cmd_queue.h>
#include <hobbes_db.h>
#include <hobbes_util.h>

#include <v3vee.h>
#include <v3_ioctl.h>

#include <pet_ioctl.h>
#include <pet_log.h>
#include <pet_xml.h>
#include <pet_mem.h>

#include "palacios.h"
#include "hobbes_ctrl.h"


extern hdb_db_t hobbes_master_db;

static int
__hobbes_launch_vm(hcq_handle_t hcq,
		   hcq_cmd_t    cmd)
{
    hobbes_id_t enclave_id     = -1;
    char      * enclave_id_str = NULL;

    pet_xml_t   xml            =  NULL;
    char      * xml_str        =  NULL;
    uint32_t    data_size      =  0;

    char      * enclave_name   =  NULL;
    int         vm_id          = -1;

    int         ret            = -1;
    char      * err_str        = NULL;


    xml_str = hcq_get_cmd_data(hcq, cmd, &data_size);

    if ((xml_str == NULL) || (xml_str[data_size - 1] != '\0')) {
	ERROR("data_size=%u, xml_str=%p\n", data_size, xml_str);
	ERROR("strlen=%lu\n", strlen(xml_str));
	ERROR("Invalid Command string=(%s)\n", xml_str);

	err_str = "Received invalid command string";
	goto out2;
    }
 
    xml = pet_xml_parse_str(xml_str);

    if (!xml) {
	err_str = "Invalid VM Spec";
	goto out2;
    }


    /* Extract meta data config fields */
    {
	enclave_name = pet_xml_get_val(xml, "name");

	if (enclave_name == NULL) {
	    err_str = "Invalid VM Spec. Missing \'name\' field\n";
	    goto out2;
	}

	enclave_id_str = pet_xml_get_val(xml, "enclave_id");
    
	if (enclave_id_str == NULL) {
	    err_str = "Invalid VM Spec. Missing \'enclave_id\' field\n";
	    goto out2;
	}
	
	enclave_id = smart_atoi(-1, enclave_id_str);
	
	if (enclave_id == -1) {
	    err_str = "Invalid VM Spec. Invalid \'enclave_id\' field";
	    goto out2;
	} 

    }

    printf("enclave_id= %d\n", enclave_id);
 

    printf("loading VM\n");
    /* Load VM Image */
    {
	u8 * img_data = NULL;
	u32  img_size = 0;

	img_data = v3_build_vm_image(xml, &img_size);

	if (img_data) {
	    printf("Creating VM\n");
	    vm_id = v3_create_vm(enclave_name, img_data, img_size);
	    
	    if (vm_id == -1) {
		err_str = "Could not create VM";
		ERROR("Could not create VM (%s)\n", enclave_name);
	    }
	} else {
	   err_str = "Could not build VM image from xml";
	}


	/* Cleanup if there was an error */
       	if ((img_data == NULL) || 
	    (vm_id    == -1)) {
	    
	    goto out1;
	}


	hdb_set_enclave_dev_id(hobbes_master_db, enclave_id, vm_id);
    }

    printf("launching VM\n");

    /* Launch VM */
    {
	ret = v3_launch_vm(vm_id);

	if (ret != 0) {
	    err_str = "Could not launch VM enclave";
	    ERROR("ERROR ERROR ERROR: We really need to implement this: v3_free(vm_id);\n");

	    hdb_set_enclave_state(hobbes_master_db, enclave_id, ENCLAVE_CRASHED);
	    
	    goto out1;
	}

    }

 out1:
    pet_xml_free(xml);
    
 out2:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;
}

static int
__hobbes_destroy_vm(hcq_handle_t hcq, 
		    hcq_cmd_t    cmd)
{
    enclave_type_t type       =  INVALID_ENCLAVE;
    hobbes_id_t  * enclave_id =  NULL;
    hobbes_id_t    parent_id  =  HOBBES_INVALID_ID;
    uint32_t       data_size  =  0;

    int            vm_id      = -1; 
    
    int            ret        = -1;
    char         * err_str    =  NULL;


    enclave_id = hcq_get_cmd_data(hcq, cmd, &data_size);

    if (data_size != sizeof(hobbes_id_t)) {
	err_str = "Enclave ID is corrupt";
	goto out;
    } 
   
    /* Double check the enclave type is correct */

    type = hobbes_get_enclave_type(*enclave_id);

    if (type != VM_ENCLAVE) {
	err_str = "Enclave is not a VM";
	goto out;
    }

    /* Double check that the VM is hosted by us */
    parent_id = hobbes_get_enclave_parent(*enclave_id);
    
    if (parent_id != hobbes_get_my_enclave_id()) {
	err_str = "VM is not hosted by this enclave\n";
	goto out;
    }

    /* Get local VM ID */

    vm_id = hobbes_get_enclave_dev_id(*enclave_id);
    
    if (vm_id == -1) {
	err_str = "Could not find VM instance";
	goto out;
    }

    /* Stop VM */

    ret = v3_stop_vm(vm_id);
    
    if (ret == -1) {
	err_str = "Could not stop VM";
	hobbes_set_enclave_state(*enclave_id, ENCLAVE_ERROR);
	goto out;
    }

    /* Free VM */
    
    ret = v3_free_vm(vm_id);

    if (ret == -1) {
	err_str = "Could not free VM";
	hobbes_set_enclave_state(*enclave_id, ENCLAVE_ERROR);
	goto out;
    }

    /* Close vm interface */
    close(vm_id);


 out:
    if (err_str) ERROR("%s\n", err_str);

    hcq_cmd_return(hcq, cmd, ret, smart_strlen(err_str) + 1, err_str);
    return 0;

}


int
palacios_init(void)
{

    // Register Hobbes commands
    hobbes_register_cmd(HOBBES_CMD_VM_LAUNCH,  __hobbes_launch_vm  );
    hobbes_register_cmd(HOBBES_CMD_VM_DESTROY, __hobbes_destroy_vm );

     return 0;
}

bool 
palacios_is_available(void)
{
    return (v3_is_vmm_present() != 0);
}



int
ensure_valid_host_memory(void)
{
    unsigned long start_page, end_page, page;
    char  * host_file = "/proc/xpmem/host_page_range\0";
    char  * line      = NULL;
    char  * page_line = NULL;
    char  * orig      = NULL;
    FILE  * fp	      = NULL;

    size_t  size   = 0;
    int     status = 0;

    fp = fopen(host_file, "r");
    if (fp == NULL) {
	if (errno == ENOENT)
	    return 0;

	printf("Cannot open %s: %s\n", host_file, strerror(errno));
	return -errno;
    }

    status = getline(&line, &size, fp);
    if (status == -1) {
	printf("Cannot read %s: %s\n", host_file, strerror(errno));
	return -errno;
    }

    orig = line;

    page_line = strsep(&line, "-");
    if (page_line == NULL || line == NULL) {
	printf("Failed to strsep %s\n", line);
	free(line);
	return -1;
    }

    start_page = strtoul(page_line, NULL, 16);
    end_page   = strtoul(line, NULL, 16);

    free(orig);
    fclose(fp);

    printf("Probing XEMEM host memory region ([0x%lx:0x%lx))\n",
	start_page, end_page);

    for (page = start_page; page < end_page; page += pet_block_size()) {
	uint32_t block = pet_addr_to_block_id((uintptr_t)page);
	pet_online_block(block);
	pet_offline_block(block);
    }

    return 0;
}
