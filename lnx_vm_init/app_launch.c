/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>


#include <stdint.h>


#include <pet_log.h>
#include <pet_xml.h>

#include <hobbes.h>
#include <hobbes_process.h>
#include <hobbes_db.h>
#include <hobbes_util.h>
#include <hobbes_cmd_queue.h>

extern hdb_db_t hobbes_master_db;


#include "app_launch.h"

#define DEFAULT_ENVP            ""
#define DEFAULT_ARGV            ""

int 
launch_lnx_app(char        * name, 
	       char        * exe_path, 
	       char        * argv, 
	       char        * envp)
{
    char * cmd_line = NULL;
    
    asprintf(&cmd_line, "%s %s %s", envp, exe_path, argv); 


    popen(cmd_line, "r");

    free(cmd_line);

    return 0;
}



int 
launch_hobbes_lnx_app(char * spec_str)
{
    pet_xml_t spec = NULL;
    int       ret  = -1;    /* This is only set to 0 if the function completes successfully */

    hobbes_id_t hobbes_process_id = HOBBES_INVALID_ID;


    spec = pet_xml_parse_str(spec_str);

    if (!spec) {
	ERROR("Invalid App spec\n");
	return -1;
    }

    {
	char        * name       = NULL; 
	char        * exe_path   = NULL; 
	char        * argv       = DEFAULT_ARGV;
	char        * envp       = DEFAULT_ENVP; 
	char        * hobbes_env = NULL;
	
	char        * val_str    = NULL;



	printf("App spec str = (%s)\n", spec_str);

	/* Executable Path Name */
	val_str = pet_xml_get_val(spec, "path");

	if (val_str) {
	    exe_path = val_str;
	} else {
	    ERROR("Missing required path in Hobbes APP specification\n");
	    goto out;
	}

	/* Process Name */
	val_str = pet_xml_get_val(spec, "name");

	if (val_str) {
	    name = val_str;
	} else {
	    name = exe_path;
	}


	/* ARGV */
	val_str = pet_xml_get_val(spec, "argv");

	if (val_str) {
	    argv = val_str;
	}

	/* ENVP */
	val_str = pet_xml_get_val(spec, "envp");

	if (val_str) {
	    envp = val_str;
	}



	/* Register as a hobbes process */
	{
	    	    
	    int ret = 0;

	    hobbes_process_id = hdb_create_process(hobbes_master_db, name, hobbes_get_my_enclave_id());

	    printf("Launching App (Hobbes process ID = %u) (EnclaveID=%d)\n", hobbes_process_id, hobbes_get_my_enclave_id() );
	    printf("process Name=%s\n", name);

	    /* Hobbes enabled ENVP */
	    ret = asprintf(&hobbes_env, 
			   "%s=%u %s=%u %s", 
			   HOBBES_ENV_PROCESS_ID,
			   hobbes_process_id, 
			   HOBBES_ENV_ENCLAVE_ID,
			   hobbes_get_my_enclave_id(), 
			   envp);

	    if (ret == -1) {
		ERROR("Failed to allocate envp string for application (%s)\n", name);
		goto out;
	    }
	    
	}
	
	/* Launch App */
	ret = launch_lnx_app(name, 
			     exe_path, 
			     argv,
			     hobbes_env);

	free(hobbes_env);

	if (ret == -1) {
	    ERROR("Failed to Launch application (spec_str=[%s])\n", spec_str);
	    goto out;
	}
    }


 out:
    pet_xml_free(spec);
    return ret;
}
