/* Hobbes Application management library 
 * (c) 2015,  Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_APP_H__
#define __HOBBES_APP_H__

#include <stdint.h>
#include <ezxml.h>

#include "hobbes.h"

typedef ezxml_t hobbes_app_spec_t;

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
		      uint64_t    stack_size);

void hobbes_free_app_spec(hobbes_app_spec_t spec);

hobbes_app_spec_t hobbes_load_app_spec(char * filename);
int hobbes_save_app_spec(hobbes_app_spec_t spec, char * filename);


int hobbes_launch_app(hobbes_id_t enclave_id, hobbes_app_spec_t app_spec);




#endif
