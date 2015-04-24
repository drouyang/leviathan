/* Hobbes Application management library 
 * (c) 2015,  Jack Lange <jacklange@cs.pitt.edu>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes_db.h"
#include "hobbes_app.h"
#include "hobbes_util.h"









hobbes_app_spec_t
hobbes_build_app_spec(char      * name, 
		      char      * exe_path,
		      char      * exe_argv,
		      char      * envp,
		      char      * cpu_list,
		      uint8_t     use_large_pages,
		      uint8_t     use_smartmap,
		      uint8_t     num_ranks,
		      uint64_t    heap_size,
		      uint64_t    stack_size)
{
    pet_xml_t   root_xml = NULL;
    char      * tmp_str  = NULL;

    root_xml = pet_xml_new_tree("app");

    if ( exe_path == NULL ) {
	ERROR("Must specify an executable at minimum\n");
	return NULL;
    }

    /* Set path */
    pet_xml_add_val(root_xml, "path", exe_path);

    /* Set name */
    if ( name ) {
	pet_xml_add_val(root_xml, "name", name);
    }

    /* Set ARGV */
    if ( exe_argv ) {
	pet_xml_add_val(root_xml, "argv", exe_argv);
    }

    /* Set envp */
    if ( envp ) {
	pet_xml_add_val(root_xml, "envp", envp);
    }

    /* Set ranks */
    if ( num_ranks > 0 ) {
        asprintf(&tmp_str, "%u", num_ranks);

	pet_xml_add_val(root_xml, "ranks", tmp_str);
    }

    /* Set cpu_list */
    if ( cpu_list ) {
	pet_xml_add_val(root_xml, "cpu_list", cpu_list);
    }

    /* Set large pages flag */
    if ( use_large_pages ) {
	pet_xml_add_val(root_xml, "use_large_pages", "1");
    } 

    /* Set smartmap flag */
    if ( use_smartmap ) {
	pet_xml_add_val(root_xml, "use_smartmap", "1");
    }

    /* Set heap size */
    if ( heap_size > 0 ) {
	asprintf(&tmp_str, "%lu", heap_size);
	
	pet_xml_add_val(root_xml, "heap_size", tmp_str);
    }

    /* Set stack size */
    if ( stack_size > 0 ) {
	asprintf(&tmp_str, "%lu", stack_size);

	pet_xml_add_val(root_xml, "stack_size", tmp_str);
    }

    free(tmp_str);
    return (hobbes_app_spec_t)root_xml;
}


hobbes_app_spec_t 
hobbes_parse_app_spec(char * xml_str)
{
    pet_xml_t spec = NULL;

    spec = pet_xml_parse_str(xml_str);
 
    if (spec == NULL) {
	ERROR("Could not parse XML input string\n");
    }

    return (hobbes_app_spec_t)spec;
}


hobbes_app_spec_t
hobbes_load_app_spec(char * filename)
{
    pet_xml_t spec_xml = NULL;

    spec_xml = pet_xml_open_file(filename);

    if (spec_xml == NULL) {
	ERROR("Could not read spec file from disk\n");
	return NULL;
    }

    if ( !pet_xml_get_val(spec_xml, "path") ) {
	ERROR("Invalid App Spec file (missing path)\n");
	return NULL;
    }

    return (hobbes_app_spec_t)spec_xml;
}


int 
hobbes_save_app_spec(hobbes_app_spec_t   spec, 
		     char              * filename)
{
    char * xml_str = NULL;
    FILE * xml_fp  = NULL;


    xml_fp = fopen(filename, "r+");

    if (xml_fp == NULL) {
	ERROR("Could not open file (%s) to save app spec\n", filename);
	return -1;
    }
    
    xml_str = pet_xml_get_str(spec);

    if (xml_str == NULL) {
	ERROR("Could not convert app spec to XML string\n");
	fclose(xml_fp);
	return -1;
    }

    fprintf(xml_fp, "%s\n", xml_str);

    free(xml_str);
    fclose(xml_fp);

    return 0;
}

void 
hobbes_free_app_spec(hobbes_app_spec_t app_spec)
{
    pet_xml_free(app_spec);
}


int 
hobbes_launch_app(hobbes_id_t       enclave_id,
		  hobbes_app_spec_t spec)
{
    hcq_handle_t  hcq      = hobbes_open_enclave_cmdq(enclave_id);
    hcq_cmd_t     cmd      = HCQ_INVALID_CMD;
    char        * spec_str = NULL;
    int           ret      = 0;

    printf("launching app\n");

    spec_str = pet_xml_get_str(spec);
    
    if (spec_str == NULL) {
	ERROR("Could not convert XML spec to string\n");
	return -1;
    }

    printf("spec_str=%s\n", spec_str);

    cmd = hcq_cmd_issue(hcq, HOBBES_CMD_APP_LAUNCH, smart_strlen(spec_str) + 1, spec_str);
    
    free(spec_str);

    ret = hcq_get_ret_code(hcq, cmd);

    hcq_cmd_complete(hcq, cmd);
    hobbes_close_enclave_cmdq(hcq);

    return ret;
}


