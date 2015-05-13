/* Hobbes Process management library 
 * (c) 2015,  Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_PROCESS_H__
#define __HOBBES_PROCESS_H__

#include <stdint.h>
#include <pet_xml.h>

#include "hobbes.h"



typedef enum {
    PROCESS_INITTED   = 0,
    PROCESS_RUNNING   = 1,
    PROCESS_STOPPED   = 2,
    PROCESS_CRASHED   = 3,
    PROCESS_ERROR     = 4
} process_state_t;


int hobbes_set_process_state(hobbes_id_t     process_id,
			     process_state_t state);

process_state_t hobbes_get_process_state(hobbes_id_t process_id);

hobbes_id_t hobbes_get_process_enclave(hobbes_id_t process_id);

char * hobbes_get_process_name(hobbes_id_t process_id);


struct process_info {
    hobbes_id_t id;
    
    char name[32];
    
    process_state_t state;
    hobbes_id_t enclave_id;
};

struct process_info *
hobbes_get_process_list(int * num_processes);



const char *
process_state_to_str(process_state_t state);


#endif
