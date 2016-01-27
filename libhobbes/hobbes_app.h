/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_APP_H__
#define __HOBBES_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pet_xml.h>

#include "hobbes.h"

typedef pet_xml_t hobbes_app_spec_t;

typedef enum {
    APP_INITTED   = 0,
    APP_RUNNING   = 1,
    APP_STOPPED   = 2,
    APP_CRASHED   = 3,
    APP_ERROR     = 4
} app_state_t;


hobbes_app_spec_t
hobbes_build_app_spec(char        * name, 
		      char        * exe_path,
		      char        * exe_argv,
		      char        * envp,
		      char        * cpu_list, 
		      uint8_t       use_large_pages,
		      uint8_t       use_smartmap,
		      uint8_t       num_ranks,
		      uint64_t      heap_size,
		      uint64_t      stack_size,
		      hobbes_id_t   app_id);

void hobbes_free_app_spec(hobbes_app_spec_t spec);

hobbes_app_spec_t hobbes_parse_app_spec(char * xml_str);
hobbes_app_spec_t hobbes_load_app_spec(char * filename);
int hobbes_save_app_spec(hobbes_app_spec_t spec, char * filename);


hobbes_id_t hobbes_create_app(char * name, hobbes_id_t enclave_id, hobbes_id_t hio_app_id);
int hobbes_free_app(hobbes_id_t app_id);

int hobbes_launch_app(hobbes_id_t enclave_id, hobbes_app_spec_t app_spec);


int hobbes_set_app_state(hobbes_id_t app_id,
			 app_state_t state);

app_state_t hobbes_get_app_state(hobbes_id_t app_id);

hobbes_id_t hobbes_get_app_enclave(hobbes_id_t app_id);

char * hobbes_get_app_name(hobbes_id_t app_id);


struct app_info {
    hobbes_id_t id;
    
    char name[32];
    
    app_state_t state;
    hobbes_id_t enclave_id;
};

struct app_info *
hobbes_get_app_list(int * num_apps);



const char *
app_state_to_str(app_state_t state);


#ifdef __cplusplus
}
#endif

#endif
