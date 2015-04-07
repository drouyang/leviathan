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


#if 0
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

#endif


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
    return root_xml;
}



int 
hobbes_launch_app(hobbes_id_t       enclave_id,
		  hobbes_app_spec_t spec)
{


    return 0;
}
