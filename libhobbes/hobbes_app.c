/* Hobbes Application management library 
 * (c) 2015,  Jack Lange <jacklange@cs.pitt.edu>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <pet_log.h>
#include <ezxml.h>

#include "hobbes_db.h"
#include "hobbes_app.h"



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


/*
static ezxml_t 
get_subtree(ezxml_t   tree,
	    char    * tag) 
{
    return ezxml_child(tree, tag);
}
*/



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
    ezxml_t   root_xml = NULL;
    ezxml_t   tmp_xml  = NULL;
    char    * tmp_str  = NULL;

    root_xml = ezxml_new_d("app");

    if ( exe_path == NULL ) {
	ERROR("Must specify an executable at minimum\n");
	return NULL;
    }

    /* Set path */
    tmp_xml = ezxml_add_child_d(root_xml, "path", 0);
    ezxml_set_txt_d(tmp_xml, exe_path);

    /* Set name */
    if ( name ) {
	tmp_xml = ezxml_add_child_d(root_xml, "name", 0);
	ezxml_set_txt_d(tmp_xml, name);
    }

    /* Set ARGV */
    if ( exe_argv ) {
	tmp_xml = ezxml_add_child_d(root_xml, "argv", 0);
	ezxml_set_txt_d(tmp_xml, exe_argv);
    }

    /* Set envp */
    if ( envp ) {
	tmp_xml = ezxml_add_child_d(root_xml, "envp", 0);
	ezxml_set_txt_d(tmp_xml, envp);
    }

    /* Set ranks */
    if ( num_ranks > 0 ) {
	tmp_xml = ezxml_add_child_d(root_xml, "ranks", 0);

        asprintf(&tmp_str, "%u", num_ranks);
	ezxml_set_txt_d(tmp_xml, tmp_str);
    }

    /* Set cpu_list */
    if ( cpu_list ) {
	tmp_xml = ezxml_add_child_d(root_xml, "cpu_list", 0);
	ezxml_set_txt_d(tmp_xml, cpu_list);
    }

    /* Set large pages flag */
    if ( use_large_pages ) {
	tmp_xml = ezxml_add_child_d(root_xml, "use_large_pages", 0);
	ezxml_set_txt_d(tmp_xml, "1");
    } 

    /* Set smartmap flag */
    if ( use_smartmap ) {
	tmp_xml = ezxml_add_child_d(root_xml, "use_smartmap", 0);
	ezxml_set_txt_d(tmp_xml, "1");
    }

    /* Set heap size */
    if ( heap_size > 0 ) {
	tmp_xml = ezxml_add_child_d(root_xml, "heap_size", 0);

	asprintf(&tmp_str, "%lu", heap_size);
	ezxml_set_txt_d(tmp_xml, tmp_str);
    }

    /* Set stack size */
    if ( stack_size > 0 ) {
	tmp_xml = ezxml_add_child_d(root_xml, "stack_size", 0);

	asprintf(&tmp_str, "%lu", stack_size);
	ezxml_set_txt_d(tmp_xml, tmp_str);
    }

    free(tmp_str);
    return (hobbes_app_spec_t)root_xml;
}


hobbes_app_spec_t 
hobbes_parse_app_spec(char * xml_str)
{
    ezxml_t   spec      = NULL;
    char    * parse_str = NULL;
   
    parse_str = strdup(xml_str);
    
    if (!parse_str) {
	ERROR("Could not duplication XML input\n");
	return NULL;
    }

    spec = ezxml_parse_str(parse_str, strlen(parse_str) + 1);

    free(parse_str);

    if (spec == NULL) {
	ERROR("Could not parse XML input string\n");
    }

    return (hobbes_app_spec_t)spec;
}


hobbes_app_spec_t
hobbes_load_app_spec(char * filename)
{
    ezxml_t spec_xml = NULL;

    spec_xml = open_xml_file(filename);

    if (spec_xml == NULL) {
	ERROR("Could not read spec file from disk\n");
	return NULL;
    }

    if ( !get_val(spec_xml, "path") ) {
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
    
    xml_str = ezxml_toxml(spec);

    if (xml_str == NULL) {
	ERROR("Could not convert app spec to XML string\n");
	fclose(xml_fp);
	return -1;
    }

    fprintf(xml_fp, "%s\n", xml_str);

    fclose(xml_fp);

    return 0;
}

void 
hobbes_free_app_spec(hobbes_app_spec_t app_spec)
{
    ezxml_free(app_spec);
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

    spec_str = ezxml_toxml(spec);
    
    if (spec_str == NULL) {
	ERROR("Could not convert XML spec to string\n");
	return -1;
    }

    printf("spec_str=%s\n", spec_str);

    cmd = hcq_cmd_issue(hcq, HOBBES_CMD_APP_LAUNCH, strlen(spec_str) + 1, spec_str);

    printf("cmd returned\n");

    printf("getting ret code\n");
    ret = hcq_get_ret_code(hcq, cmd);
    
    printf("hcq cmd_complete\n");
    hcq_cmd_complete(hcq, cmd);
    
    printf("hobbes_close enclave cmdq\n");
    hobbes_close_enclave_cmdq(hcq);

    printf("Returning from launch app\n");
    return ret;
}


