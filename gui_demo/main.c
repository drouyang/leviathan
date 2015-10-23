/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


#include <pet_log.h>
#include <ezxml.h>

#include <hobbes.h>


bool use_topo_file = true;



static int
generate_socket_svg(ezxml_t canvas,
		    int     num_cols,
		    int     num_rows,
		    int     col_idx,
		    int     row_idx)
{

    return -1;


}

static ezxml_t 
generate_svg( void )
{
    ezxml_t svg_root = NULL;
    ezxml_t canvas   = NULL;
    
    svg_root = ezxml_new("svg");
    ezxml_set_attr_d(svg_root, "width",   "100%");
    ezxml_set_attr_d(svg_root, "height",  "100%");
    ezxml_set_attr_d(svg_root, "xmlns",   "http://www.w3.org/2000/svg");
    ezxml_set_attr_d(svg_root, "viewBox", "0 0 1000 650");
	
    canvas = ezxml_add_child(svg_root, "svg", 0);
    ezxml_set_attr_d(canvas, "x",      "1%");
    ezxml_set_attr_d(canvas, "y",      "1%");
    ezxml_set_attr_d(canvas, "width",  "98%");
    ezxml_set_attr_d(canvas, "height", "80%");
    

    /* Get Socket Info */
    {
	int socket_cnt = 0;
	int num_cols   = 0;
	int num_rows   = 0;
	int col_idx    = 0;
	int row_idx    = 0;

	int i = 0;
	
	
	for (i = 0; i < socket_cnt; i++) {
	    generate_socket_svg(canvas, num_cols, num_rows, col_idx, row_idx);
	}
	    
    }

    return svg_root;
}


int main(int argc, char ** argv) {
    ezxml_t svg_xml = NULL;
    char  * xml_str = NULL;

    svg_xml = generate_svg();

    xml_str = ezxml_toxml(svg_xml);


    printf("%s\n", xml_str);

    free(xml_str);
    ezxml_free(svg_xml);


    return 0;
}
