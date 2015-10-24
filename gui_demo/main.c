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
#include <math.h>
#include <fenv.h>



#include <pet_log.h>
#include <ezxml.h>


#include <hobbes.h>
#include <hobbes_util.h>
#include <hobbes_system.h>


bool use_topo_file = true;

struct hobbes_memory_info * sys_mem_blk_info = NULL;
struct hobbes_cpu_info    * sys_cpu_info     = NULL;
uint64_t sys_mem_blk_cnt = 0;
uint32_t sys_cpu_cnt = 0;


static const char *
__get_mem_fill_str(struct hobbes_memory_info * info)
{

    if ((info->state == MEMORY_ALLOCATED) && 
	(info->numa_node == -1)) {
	return "black";
    } else if (info->state == MEMORY_FREE) {
	return "lightgrey";
    } else if (info->state == MEMORY_ALLOCATED) {
	if ((info->enclave_id == HOBBES_MASTER_ENCLAVE_ID) ||
	    (info->enclave_id == -1)) {
	    return "green";
	}
    }

    return "red";
}


static int 
__get_numa_blk_cnt(int numa_idx)
{
    int blk_cnt = 0;
    int i       = 0;

    for (i = 0; i < sys_mem_blk_cnt; i++) {
	if (sys_mem_blk_info[i].numa_node == numa_idx) {
	    blk_cnt++;
	}
    }

    return blk_cnt;
}


static int
__get_numa_cpu_cnt(int numa_idx)
{
    int cpu_cnt = 0;
    int i       = 0;

    for (i = 0; i < sys_cpu_cnt; i++) {
	if (sys_cpu_info[i].numa_node == numa_idx) {
	    cpu_cnt++;
	}
    }

    return cpu_cnt;
}


struct hobbes_memory_info *
__get_mem_blk_entry(int numa_idx, 
		    int blk_idx)
{
    int i = 0;
    
    for (i = 0; i < sys_mem_blk_cnt; i++) {
	if (sys_mem_blk_info[i].numa_node != numa_idx) continue;

	if (blk_idx == 0) {
	    return &sys_mem_blk_info[i];
	} else {
	    blk_idx--;
	}
    }
    
    return NULL;
}

static int
generate_mem_svg(ezxml_t numa_canvas,
		 int     numa_idx)
{
    ezxml_t mem_svg    = NULL;
    ezxml_t mem_rect   = NULL;
    ezxml_t mem_canvas = NULL;
    ezxml_t mem_label  = NULL;
    ezxml_t label_svg  = NULL;

    char * tmp_str = NULL;
    int    blk_cnt = __get_numa_blk_cnt(numa_idx);

    int i = 0;

    mem_svg = ezxml_add_child(numa_canvas, "svg", 0);
    ezxml_set_attr_d(mem_svg, "x",      "3%");
    ezxml_set_attr_d(mem_svg, "y",      "7.5%");
    ezxml_set_attr_d(mem_svg, "width",  "40%");
    ezxml_set_attr_d(mem_svg, "height", "85%");


    mem_rect = ezxml_add_child(mem_svg, "rect", 0);
    ezxml_set_attr_d(mem_rect, "x",      "1%");
    ezxml_set_attr_d(mem_rect, "y",      "1%");
    ezxml_set_attr_d(mem_rect, "width",  "98%");
    ezxml_set_attr_d(mem_rect, "height", "98%");
    ezxml_set_attr_d(mem_rect, "fill",   "white");
    ezxml_set_attr_d(mem_rect, "stroke", "black");
    ezxml_set_attr_d(mem_rect, "stroke-width", "2");

    mem_canvas = ezxml_add_child(mem_svg, "svg", 1);
    ezxml_set_attr_d(mem_canvas, "x", "2%");
    ezxml_set_attr_d(mem_canvas, "y", "2%");
    ezxml_set_attr_d(mem_canvas, "width", "96%");
    ezxml_set_attr_d(mem_canvas, "height", "96%");

    asprintf(&tmp_str, "0 0 100 %d", blk_cnt);
    ezxml_set_attr_d(mem_canvas, "viewBox", tmp_str);
    smart_free(tmp_str);

    ezxml_set_attr_d(mem_canvas, "preserveAspectRatio", "none");

    for (i = 0; i < blk_cnt; i++) {
	struct hobbes_memory_info * entry = __get_mem_blk_entry(numa_idx, i);
	ezxml_t blk_rect = NULL;

	blk_rect = ezxml_add_child(mem_canvas, "rect", i);
	ezxml_set_attr_d(blk_rect, "x", "0");
	
	asprintf(&tmp_str, "%d", i);
	ezxml_set_attr_d(blk_rect, "y", tmp_str);
	smart_free(tmp_str);
       
	ezxml_set_attr_d(blk_rect, "width",  "100%");
	ezxml_set_attr_d(blk_rect, "height", "1");
	ezxml_set_attr_d(blk_rect, "fill",   __get_mem_fill_str(entry));

    }

    label_svg = ezxml_add_child(numa_canvas, "svg", 1);
    ezxml_set_attr_d(label_svg, "x",                   "7%");
    ezxml_set_attr_d(label_svg, "y",                   "92.5%");
    ezxml_set_attr_d(label_svg, "width",               "32%");
    ezxml_set_attr_d(label_svg, "height",              "120%");
    ezxml_set_attr_d(label_svg, "viewBox",             "0 0 100 350");
    ezxml_set_attr_d(label_svg, "preserveAspectRatio", "none");


    mem_label = ezxml_add_child(label_svg, "text", 1);
    ezxml_set_attr_d(mem_label, "x",           "50%");
    ezxml_set_attr_d(mem_label, "y",           "0%");
    ezxml_set_attr_d(mem_label, "dy",          ".75em");
    ezxml_set_attr_d(mem_label, "text-anchor", "middle");
    ezxml_set_attr_d(mem_label, "font-weight", "bold");
    ezxml_set_attr_d(mem_label, "fill",        "white");
    ezxml_set_txt(mem_label, "Mem Blocks");

    return 0;

}

static int
generate_cpu_svg(ezxml_t numa_canvas,
		 int     numa_idx)
{
    ezxml_t cpu_svg       = NULL;
    ezxml_t cpu_rect      = NULL;
    ezxml_t cpu_canvas    = NULL;
    ezxml_t cpu_label_svg = NULL;
    ezxml_t cpu_label_txt = NULL;

    cpu_svg = ezxml_add_child(numa_canvas, "svg", 2);
    ezxml_set_attr_d(cpu_svg, "x",      "45%");
    ezxml_set_attr_d(cpu_svg, "y",      "25%");
    ezxml_set_attr_d(cpu_svg, "width",  "50%");
    ezxml_set_attr_d(cpu_svg, "height", "50%");

    
    cpu_rect = ezxml_add_child(cpu_svg, "rect", 0);
    ezxml_set_attr_d(cpu_rect, "x",      "1%");
    ezxml_set_attr_d(cpu_rect, "y",      "1%");
    ezxml_set_attr_d(cpu_rect, "rx",     "10%");
    ezxml_set_attr_d(cpu_rect, "ry",     "10%");
    ezxml_set_attr_d(cpu_rect, "width",  "98%");
    ezxml_set_attr_d(cpu_rect, "height", "98%");
    ezxml_set_attr_d(cpu_rect, "fill",   "white");
    ezxml_set_attr_d(cpu_rect, "stroke", "black");

    cpu_canvas = ezxml_add_child(cpu_svg, "svg", 1);
    ezxml_set_attr_d(cpu_canvas, "x",      "2%");
    ezxml_set_attr_d(cpu_canvas, "y",      "2%");
    ezxml_set_attr_d(cpu_canvas, "height", "96%");
    ezxml_set_attr_d(cpu_canvas, "width",  "96%");
    ezxml_set_attr_d(cpu_canvas, "viewBox", "0 0 1000 1000");


    {
	int cpu_cnt  = __get_numa_cpu_cnt(numa_idx);
	int num_cols = 0;
	int num_rows = 0;

	int i = 0;
	
	if (cpu_cnt <= 0) {
	    goto no_cpus;
	}

	fesetround(FE_UPWARD);
	num_cols = (int)nearbyint(sqrt(cpu_cnt));
	num_rows = (cpu_cnt / num_cols) + ((cpu_cnt % num_cols) != 0);


	for (i = 0; i < cpu_cnt; i++) {
	    ezxml_t cpu_circle = NULL;
	    ezxml_t label_svg  = NULL;
	    ezxml_t label_txt  = NULL;

	    char * tmp_str     = NULL;
	    int    col_idx     = i % num_cols;
	    int    row_idx     = i / num_cols;

	    double cx = (((100.0 / num_cols) / 2) * (1 + (2 * col_idx)));
	    double cy = (((100.0 / num_rows) / 2) * (1 + (2 * row_idx)));;
	    double r  = ((100.0 / num_cols) / 2) - 2;

	    cpu_circle = ezxml_add_child(cpu_canvas, "circle", i * 2);

	    asprintf(&tmp_str, "%.2f%%", cx);
	    ezxml_set_attr_d(cpu_circle, "cx", tmp_str);
	    smart_free(tmp_str);

	    asprintf(&tmp_str, "%.2f%%", cy);
	    ezxml_set_attr_d(cpu_circle, "cy", tmp_str);
	    smart_free(tmp_str);

	    asprintf(&tmp_str, "%.2f%%", r);
	    ezxml_set_attr_d(cpu_circle, "r", tmp_str);
	    smart_free(tmp_str);

	    ezxml_set_attr_d(cpu_circle, "fill",   "blue");
	    ezxml_set_attr_d(cpu_circle, "stroke", "black");
	    ezxml_set_attr_d(cpu_circle, "stroke-width", "10");
	    

	    label_svg = ezxml_add_child(cpu_canvas, "svg", (i * 2) + 1);
	    asprintf(&tmp_str, "%.2f%%", cx - r);
	    ezxml_set_attr_d(label_svg, "x", tmp_str);
	    smart_free(tmp_str);

	    asprintf(&tmp_str, "%.2f%%", cy - r);
	    ezxml_set_attr_d(label_svg, "y", tmp_str);
	    smart_free(tmp_str);

	    asprintf(&tmp_str, "%.2f%%", r * 2);
	    ezxml_set_attr_d(label_svg, "width",  tmp_str);
	    ezxml_set_attr_d(label_svg, "height", tmp_str);
	    smart_free(tmp_str);

	    ezxml_set_attr_d(label_svg, "viewBox", "0 0 40 40");

	    label_txt = ezxml_add_child(label_svg, "text", 0);
	    ezxml_set_attr_d(label_txt, "x",           "50%");
	    ezxml_set_attr_d(label_txt, "y",           "0");
	    ezxml_set_attr_d(label_txt, "dy",          "60%");
	    ezxml_set_attr_d(label_txt, "text-anchor", "middle");
	    ezxml_set_attr_d(label_txt, "fill",       "white");

	    
	    asprintf(&tmp_str, "%d", i);
	    ezxml_set_txt_d(label_txt, tmp_str);
	    smart_free(tmp_str);
	    
	}
    }
 no_cpus:
    
    cpu_label_svg = ezxml_add_child(numa_canvas, "svg", 3);
    ezxml_set_attr_d(cpu_label_svg, "x",        "49%");
    ezxml_set_attr_d(cpu_label_svg, "y",        "75%");
    ezxml_set_attr_d(cpu_label_svg, "width",    "42%");
    ezxml_set_attr_d(cpu_label_svg, "height",   "120%");
    ezxml_set_attr_d(cpu_label_svg, "viewBox",  "0 0 100 300");
    ezxml_set_attr_d(cpu_label_svg, "preserveAspectRatio", "none");

    cpu_label_txt = ezxml_add_child(cpu_label_svg, "text", 0);
    ezxml_set_attr_d(cpu_label_txt, "x",           "50%");
    ezxml_set_attr_d(cpu_label_txt, "y",           "0");
    ezxml_set_attr_d(cpu_label_txt, "dy",          ".75em");
    ezxml_set_attr_d(cpu_label_txt, "text-anchor", "middle");
    ezxml_set_attr_d(cpu_label_txt, "font-weight", "bold");
    ezxml_set_attr_d(cpu_label_txt, "font-size",   "14");
    ezxml_set_attr_d(cpu_label_txt, "fill",        "white");
    ezxml_set_txt_d(cpu_label_txt, "CPU Cores");



    return 0;
}


static int
generate_numa_svg(ezxml_t canvas,
		  int     numa_idx,
		  int     num_cols,
		  int     num_rows)
{
    ezxml_t numa_svg  = NULL;
    ezxml_t numa_rect = NULL;
    char  * tmp_str   = NULL;

    double  width     = (100.0 / num_cols);
    double  height    = (100.0 / num_rows);

    int     col_idx   = numa_idx % num_cols;
    int     row_idx   = numa_idx / num_cols;

    double  x         = (width  * col_idx) + 1;
    double  y         = (height * row_idx) + 1;


    printf("%d, %d\n", col_idx, row_idx);

    numa_svg = ezxml_add_child(canvas, "svg", numa_idx);

    asprintf(&tmp_str, "%.2f%%", x);
    ezxml_set_attr_d(numa_svg, "x", tmp_str);
    smart_free(tmp_str);

    asprintf(&tmp_str, "%.2f%%", y);
    ezxml_set_attr_d(numa_svg, "y", tmp_str);
    smart_free(tmp_str);

    asprintf(&tmp_str, "%.2f%%", width - 2);
    ezxml_set_attr_d(numa_svg, "width", tmp_str);
    smart_free(tmp_str);
    
    asprintf(&tmp_str, "%.2f%%", height - 2);
    ezxml_set_attr_d(numa_svg, "height", tmp_str);
    smart_free(tmp_str);

  


    numa_rect = ezxml_add_child(numa_svg, "rect", 0);
    ezxml_set_attr_d(numa_rect, "x",      "1%");
    ezxml_set_attr_d(numa_rect, "y",      "1%");
    ezxml_set_attr_d(numa_rect, "rx",     "10%");
    ezxml_set_attr_d(numa_rect, "ry",     "10%");
    ezxml_set_attr_d(numa_rect, "width",  "98%");
    ezxml_set_attr_d(numa_rect, "height", "98%");
    ezxml_set_attr_d(numa_rect, "fill",   "black");
    ezxml_set_attr_d(numa_rect, "stroke", "black");
    ezxml_set_attr_d(numa_rect, "style",  "stroke-width:0.5%;opacity:0.5");
    

    generate_mem_svg(numa_svg, numa_idx);

    generate_cpu_svg(numa_svg, numa_idx);
    
    return 0;


}

static ezxml_t 
generate_svg( void )
{
    ezxml_t svg_root = NULL;
    ezxml_t canvas   = NULL;

    sys_mem_blk_info = hobbes_get_memory_list(&sys_mem_blk_cnt);
    sys_cpu_info     = hobbes_get_cpu_list(&sys_cpu_cnt);
 
    svg_root = ezxml_new("svg");
    ezxml_set_attr_d(svg_root, "width",   "100%");
    ezxml_set_attr_d(svg_root, "height",  "100%");
    ezxml_set_attr_d(svg_root, "xmlns",   "http://www.w3.org/2000/svg");
    ezxml_set_attr_d(svg_root, "viewBox", "0 0 1000 1000");
	
    canvas = ezxml_add_child(svg_root, "svg", 0);
    ezxml_set_attr_d(canvas, "x",      "1%");
    ezxml_set_attr_d(canvas, "y",      "1%");
    ezxml_set_attr_d(canvas, "width",  "98%");
    ezxml_set_attr_d(canvas, "height", "90%");
    

    /* Get Numa Info */
    {
	int numa_cnt = hobbes_get_numa_cnt();
	int num_cols = 0;
	int num_rows = 0;

	int i = 0;
	
	fesetround(FE_UPWARD);
	num_cols = (int)nearbyint(sqrt(numa_cnt));
	num_rows = (numa_cnt / num_cols) + ((numa_cnt % num_cols) != 0);


	printf("Num_cols = %d\n", num_cols);
	for (i = 0; i < numa_cnt; i++) {

	    generate_numa_svg(canvas, i, num_cols, num_rows);
	}
	
    }

    return svg_root;
}


int main(int argc, char ** argv) {
    ezxml_t svg_xml = NULL;
    char  * xml_str = NULL;

    if (hobbes_client_init() != 0) {
        ERROR("Could not initialize hobbes client\n");
        return -1;
    }



    svg_xml = generate_svg();

    xml_str = ezxml_toxml(svg_xml);


    printf("%s\n", xml_str);

    free(xml_str);
    ezxml_free(svg_xml);


    hobbes_client_deinit();



    return 0;
}
