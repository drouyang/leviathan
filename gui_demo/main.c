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
#include <hobbes_enclave.h>


bool use_topo_file = true;

struct hobbes_memory_info * sys_mem_blk_info = NULL;
struct hobbes_cpu_info    * sys_cpu_info     = NULL;
struct enclave_info       * enclaves         = NULL;


uint64_t sys_mem_blk_cnt = 0;
uint32_t sys_cpu_cnt     = 0;
uint32_t enclave_cnt     = 0;



struct color {
    hobbes_id_t   enclave_id;
    char        * bg_color;
    char        * font_color;
    int           use_flag;
};



#define BG_DEFAULT "white"
#define BG_MASTER  "green"
#define BG_FREE    "lightgrey"
#define BG_INVALID "black"

#define FONT_DEFAULT "black"
#define FONT_MASTER  "white"
#define FONT_FREE    "black"
#define FONT_INVALID "white"

struct color master_color = {HOBBES_MASTER_ENCLAVE_ID, BG_MASTER, FONT_MASTER, 0};

struct color enclave_colors[] = {
    {HOBBES_INVALID_ID, "mediumslateblue", "white", 0},
    {HOBBES_INVALID_ID, "lightskyblue",    "black", 0},
    {HOBBES_INVALID_ID, "darkorange",      "black", 0},
    {HOBBES_INVALID_ID, "indigo",          "white", 0},
    {HOBBES_INVALID_ID, "orangered",       "white", 0},
    {HOBBES_INVALID_ID, "royalblue",       "white", 0},
    {HOBBES_INVALID_ID, "saddlebrown",     "white", 0},
    {HOBBES_INVALID_ID, "indianred",       "white", 0},
    {HOBBES_INVALID_ID, "burlywood",       "black", 0},
    {HOBBES_INVALID_ID, "turquoise",       "black", 0},
    {HOBBES_INVALID_ID, "maroon",          "white", 0},
    {HOBBES_INVALID_ID, "olive",           "white", 0},
    {HOBBES_INVALID_ID, "teal",            "black", 0},
    {HOBBES_INVALID_ID, "gold",            "black", 0},
    {HOBBES_INVALID_ID, "yellow",          "black", 0},
    {HOBBES_INVALID_ID, "palegreen",       "black", 0},
    {HOBBES_INVALID_ID, "darkgreen",       "white", 0},
    {HOBBES_INVALID_ID, "lightpink",       "black", 0}
};





static uint32_t
__get_numa_blk_cnt(uint32_t numa_idx)
{
    uint32_t blk_cnt = 0;
    uint32_t i       = 0;

    for (i = 0; i < sys_mem_blk_cnt; i++) {
	if (sys_mem_blk_info[i].numa_node == numa_idx) {
	    blk_cnt++;
	}
    }

    return blk_cnt;
}


static uint32_t
__get_numa_cpu_cnt(uint32_t numa_idx)
{
    uint32_t cpu_cnt = 0;
    uint32_t i       = 0;

    for (i = 0; i < sys_cpu_cnt; i++) {
	if (sys_cpu_info[i].numa_node == numa_idx) {
	    cpu_cnt++;
	}
    }

    return cpu_cnt;
}


struct hobbes_memory_info *
__get_mem_blk_entry(uint32_t numa_idx, 
		    uint32_t blk_idx)
{
    uint32_t i = 0;
    
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


struct hobbes_cpu_info * 
__get_cpu_entry(uint32_t numa_idx, 
		uint32_t cpu)
{
    uint32_t i = 0;
    
    for (i = 0; i < sys_cpu_cnt; i++) {
	if (sys_cpu_info[i].numa_node != numa_idx)  continue;

	if (cpu == 0) {
	    return &sys_cpu_info[i];
	} else {
	    cpu--;
	}
    }

    return NULL;
}


static struct color * 
__alloc_free_color(hobbes_id_t enclave_id)
{
    int color_cnt   = sizeof(enclave_colors) / sizeof(struct color);
    int i = 0;

    for (i = 0; i < color_cnt; i++) {
	if (enclave_colors[i].enclave_id == HOBBES_INVALID_ID) {
	    enclave_colors[i].enclave_id = enclave_id;
	    return &enclave_colors[i];
	}
    }

    return NULL;
}


static struct color *
__get_color(hobbes_id_t enclave_id)
{
    int color_cnt   = sizeof(enclave_colors) / sizeof(struct color);
    int i = 0;

    for (i = 0; i < color_cnt; i++) {
	if (enclave_colors[i].enclave_id == enclave_id) {
	    return &enclave_colors[i];
	}
    }

    return NULL;
}


static int
__assign_colors()
{
    uint32_t color_cnt   = sizeof(enclave_colors) / sizeof(struct color);
    uint32_t i = 0;


    if (enclaves) {
	smart_free(enclaves);
	enclave_cnt  = 0;
    }


    /* Clear all use flags */
    for (i = 0; i < color_cnt; i++) {
	enclave_colors[i].use_flag = 0;
    }

    /* retrieve enclave list */
    
    enclaves = hobbes_get_enclave_list(&enclave_cnt);

    if (enclaves == NULL) {
	ERROR("Could not retrieve enclave list\n");
	return -1;
    }
    
    /* increment use flags for enclaves in list */
    for (i = 0; i < enclave_cnt; i++) {
	struct color * clr = __get_color(enclaves[i].id);
	
	if (enclaves[i].id == HOBBES_MASTER_ENCLAVE_ID) continue;
	if (!clr) continue;

	clr->use_flag = 1;
    }


    /* garbage collect unused colors */
    for (i = 0; i < color_cnt; i++) {
	struct color * clr = &enclave_colors[i];

	if ( (clr->enclave_id != HOBBES_INVALID_ID) &&
	     (clr->use_flag   == 0) ) {
	    // free color
	    clr->enclave_id = HOBBES_INVALID_ID;
	}
    }
    
    
    /* Assign colors to new enclaves */
    for (i = 0; i < enclave_cnt; i++) {
	struct color * clr = __get_color(enclaves[i].id);
	
	if (enclaves[i].id == HOBBES_MASTER_ENCLAVE_ID) continue;
	if (clr) continue;

	clr = __alloc_free_color(enclaves[i].id);

	if (!clr) {
	    ERROR("Could not locate free color for enclave\n");
	    return -1;
	}
    }  

    
    return 0;
}


static const char *
__get_mem_fill_str(uint32_t numa_idx, uint32_t blk)
{
    struct hobbes_memory_info * info = __get_mem_blk_entry(numa_idx, blk);


    if ((info->state == MEMORY_ALLOCATED) && 
	(info->numa_node == HOBBES_INVALID_NUMA_ID)) {
	return BG_INVALID;
    } else if (info->state == MEMORY_FREE) {
	return BG_FREE;
    } else if (info->state == MEMORY_ALLOCATED) {
	if ((info->enclave_id == HOBBES_MASTER_ENCLAVE_ID) ||
	    (info->enclave_id == HOBBES_INVALID_ID)) {
	    return BG_MASTER;
	} else  {
	    struct color * clr = __get_color(info->enclave_id);
	    
	    if (!clr) return BG_INVALID;
	    
	    return clr->bg_color;
	}
    }

    return BG_INVALID;
}

static const char *
__get_cpu_fill_str(uint32_t numa_idx, uint32_t cpu)
{
    struct hobbes_cpu_info * info = __get_cpu_entry(numa_idx, cpu);

    if (info->state == CPU_FREE) {
	return BG_FREE;
    } else if(info->state == CPU_ALLOCATED) {
	if ((info->enclave_id == HOBBES_MASTER_ENCLAVE_ID) ||
	    (info->enclave_id == HOBBES_INVALID_ID)) {
	    return BG_MASTER;
	} else {
	    struct color * clr = __get_color(info->enclave_id);

	    if (!clr) return BG_INVALID;

	    return clr->bg_color;
	} 
    }
	
    return BG_INVALID;
}

static const char *
__get_cpu_text_str(uint32_t numa_idx, uint32_t cpu)
{
    struct hobbes_cpu_info * info = __get_cpu_entry(numa_idx, cpu);

    if (info->state == CPU_FREE) {
	return FONT_FREE;
    } else if(info->state == CPU_ALLOCATED) {
	if ((info->enclave_id == HOBBES_MASTER_ENCLAVE_ID) ||
	    (info->enclave_id == HOBBES_INVALID_ID)) {
	    return FONT_MASTER;
	} else {
	    struct color * clr = __get_color(info->enclave_id);

	    if (!clr) return FONT_INVALID;

	    return clr->font_color;
	} 
    }
	
    return FONT_INVALID;
}





static int
generate_mem_svg(ezxml_t  numa_canvas,
		 uint32_t numa_idx)
{
    ezxml_t mem_svg    = NULL;
    ezxml_t mem_rect   = NULL;
    ezxml_t mem_canvas = NULL;
    ezxml_t mem_label  = NULL;
    ezxml_t label_svg  = NULL;

    char   * tmp_str = NULL;
    uint32_t blk_cnt = __get_numa_blk_cnt(numa_idx);

    uint32_t i = 0;

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
	ezxml_t blk_rect = NULL;

	blk_rect = ezxml_add_child(mem_canvas, "rect", i);
	ezxml_set_attr_d(blk_rect, "x", "0");
	
	asprintf(&tmp_str, "%d", i);
	ezxml_set_attr_d(blk_rect, "y", tmp_str);
	smart_free(tmp_str);
       
	ezxml_set_attr_d(blk_rect, "width",  "100%");
	ezxml_set_attr_d(blk_rect, "height", "1");
	ezxml_set_attr_d(blk_rect, "fill",   __get_mem_fill_str(numa_idx, i));

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
generate_cpu_svg(ezxml_t  numa_canvas,
		 uint32_t numa_idx)
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
	uint32_t cpu_cnt  = __get_numa_cpu_cnt(numa_idx);
	uint32_t num_cols = 0;
	uint32_t num_rows = 0;

	uint32_t i = 0;
	
	if (cpu_cnt <= 0) {
	    goto no_cpus;
	}

	fesetround(FE_UPWARD);
	num_cols = (int)nearbyint(sqrt(cpu_cnt));
	num_rows = (cpu_cnt / num_cols) + ((cpu_cnt % num_cols) != 0);


	for (i = 0; i < cpu_cnt; i++) {
	    ezxml_t   cpu_circle  = NULL;
	    ezxml_t   label_svg   = NULL;
	    ezxml_t   label_txt   = NULL;

	    char    * tmp_str     = NULL;
	    uint32_t  col_idx     = i % num_cols;
	    uint32_t  row_idx     = i / num_cols;

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

	    ezxml_set_attr_d(cpu_circle, "fill",   __get_cpu_fill_str(numa_idx, i));
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
	    ezxml_set_attr_d(label_txt, "fill",        __get_cpu_text_str(numa_idx, i));

	    
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
generate_numa_svg(ezxml_t  canvas,
		  uint32_t numa_idx,
		  uint32_t num_cols,
		  uint32_t num_rows)
{
    ezxml_t  numa_svg  = NULL;
    ezxml_t  numa_rect = NULL;
    char   * tmp_str   = NULL;

    double   width     = (100.0 / num_cols);
    double   height    = (100.0 / num_rows);

    uint32_t col_idx   = numa_idx % num_cols;
    uint32_t row_idx   = numa_idx / num_cols;

    double   x         = (width  * col_idx) + 1;
    double   y         = (height * row_idx) + 1;


    //    printf("%d, %d\n", col_idx, row_idx);

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

static int
generate_legend_svg( ezxml_t canvas)
{
    ezxml_t legend_svg  = NULL;
    ezxml_t legend_rect = NULL;
    
    uint32_t i = 0;
    
    legend_svg = ezxml_add_child(canvas, "svg", 0);

    ezxml_set_attr_d(legend_svg, "x",                   "1%");
    ezxml_set_attr_d(legend_svg, "y",                   "91%");
    ezxml_set_attr_d(legend_svg, "width",               "98%");
    ezxml_set_attr_d(legend_svg, "height",              "8%");
    ezxml_set_attr_d(legend_svg, "viewBox",             "0 0 100 5");
    ezxml_set_attr_d(legend_svg, "preserveAspectRatio", "none");

    legend_rect = ezxml_add_child(legend_svg, "rect", 0);
    
    ezxml_set_attr_d(legend_rect, "x",            "1.5%");
    ezxml_set_attr_d(legend_rect, "y",            "1.5%");
    ezxml_set_attr_d(legend_rect, "rx",           "1%");
    ezxml_set_attr_d(legend_rect, "width",        "97%");
    ezxml_set_attr_d(legend_rect, "height",       "97%");
    ezxml_set_attr_d(legend_rect, "stroke",       "black");
    ezxml_set_attr_d(legend_rect, "fill",         "white");
    ezxml_set_attr_d(legend_rect, "stroke-width", "0.15");

 
    for (i = 0; i < enclave_cnt; i++) {
	struct color * enclave_color = __get_color(enclaves[i].id);

	ezxml_t  label_rect = NULL;
	ezxml_t  label_svg  = NULL;
	ezxml_t  label_text = NULL;

	char   * tmp_str    = NULL;

	uint32_t row = i / 5;
	uint32_t col = i % 5;


	/* We only support 10 enclaves or less in the legend */
	if (i >= 10) {
	    break;
	}

	if (enclaves[i].id == HOBBES_MASTER_ENCLAVE_ID) {
	    enclave_color = &master_color;
	} 

	if (enclave_color == NULL) {
	    ERROR("Could not find color of enclave (%d)\n", enclaves[i].id);
	    return -1;
	}
	

	label_rect = ezxml_add_child(legend_svg, "rect", 1 + (i * 2));
	label_svg  = ezxml_add_child(legend_svg, "svg",  1 + (i * 2) + 1);


	
	asprintf(&tmp_str, "%.2f%%", 2.5 + (20.0 * col));
	ezxml_set_attr_d(label_rect, "x", tmp_str);
	ezxml_set_attr_d(label_svg,  "x", tmp_str);
	smart_free(tmp_str);

	asprintf(&tmp_str, "%d%%", 5 + (50 * row));
	ezxml_set_attr_d(label_rect, "y", tmp_str);
	ezxml_set_attr_d(label_svg,  "y", tmp_str);
	smart_free(tmp_str);
	
	

	ezxml_set_attr_d(label_rect, "height", "40%");
	ezxml_set_attr_d(label_rect, "width",  "15%");
	ezxml_set_attr_d(label_rect, "fill",   enclave_color->bg_color);


	ezxml_set_attr_d(label_svg,  "height", "40%");
	ezxml_set_attr_d(label_svg,  "width",  "15%");
	

	label_text = ezxml_add_child(label_svg, "text", 0);

	ezxml_set_attr_d(label_text, "x",           "50%");
	ezxml_set_attr_d(label_text, "y",           "0");
	ezxml_set_attr_d(label_text, "dy",          "0.75em");
	ezxml_set_attr_d(label_text, "text-anchor", "middle");
	ezxml_set_attr_d(label_text, "font-weight", "bold");
	ezxml_set_attr_d(label_text, "font-size",   "2");
	ezxml_set_attr_d(label_text, "fill",        enclave_color->font_color);
	ezxml_set_txt_d(label_text, enclaves[i].name);

    }


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

	
    canvas = ezxml_add_child(svg_root, "svg", 0);
    ezxml_set_attr_d(canvas, "x",      "1%");
    ezxml_set_attr_d(canvas, "y",      "1%");
    ezxml_set_attr_d(canvas, "width",  "98%");
    ezxml_set_attr_d(canvas, "height", "90%");
    

    /* Get Numa Info */
    {
	uint32_t numa_cnt = hobbes_get_numa_cnt();
	uint32_t num_cols = 0;
	uint32_t num_rows = 0;

	uint32_t i = 0;
	
	fesetround(FE_UPWARD);
	num_cols = (int)nearbyint(sqrt(numa_cnt));
	num_rows = (numa_cnt / num_cols) + ((numa_cnt % num_cols) != 0);

	
	/* update Viewbox based on NUMA count */
	if (num_cols == 1) {
	    ezxml_set_attr_d(svg_root, "viewBox", "0 0 1000 1000");
	} else if (num_cols == 2) {
	    ezxml_set_attr_d(svg_root, "viewBox", "0 0 1000 600");
	}

	//	printf("Num_cols = %d\n", num_cols);
	for (i = 0; i < numa_cnt; i++) {

	    generate_numa_svg(canvas, i, num_cols, num_rows);
	}
	
    }

    generate_legend_svg(svg_root);


    return svg_root;
}


int main(int argc, char ** argv) {
    ezxml_t svg_xml = NULL;
    char  * xml_str = NULL;

    if (hobbes_client_init() != 0) {
        ERROR("Could not initialize hobbes client\n");
        return -1;
    }


    
    if (__assign_colors() != 0) {
	printf("Error: Could not assign enclave colors\n");
	hobbes_client_deinit();
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
