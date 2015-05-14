/* Hobbes Process Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>


#include <pet_log.h>
#include <pet_xml.h>

#include "hobbes.h"
#include "hobbes_util.h"
#include "hobbes_process.h"
#include "hobbes_db.h"

extern hdb_db_t hobbes_master_db;



int 
hobbes_set_process_state(hobbes_id_t     process_id,
			 process_state_t state)
{
    return hdb_set_process_state(hobbes_master_db, process_id, state);

}

process_state_t 
hobbes_get_process_state(hobbes_id_t process_id)
{
    return hdb_get_process_state(hobbes_master_db, process_id);
}

hobbes_id_t 
hobbes_get_process_enclave(hobbes_id_t process_id)
{
    return hdb_get_process_enclave(hobbes_master_db, process_id);
}

char * 
hobbes_get_process_name(hobbes_id_t process_id)
{
    return hdb_get_process_name(hobbes_master_db, process_id);
}


struct process_info * 
hobbes_get_process_list(int * num_processes)
{
    struct process_info * info_arr = NULL;
    hobbes_id_t         * id_arr   = NULL;

    int id_cnt = -1;
    int i      =  0;

    id_arr = hdb_get_processes(hobbes_master_db, &id_cnt);
    
    if (id_cnt == -1) {
	ERROR("could not retrieve list of process IDs\n");
	return NULL;
    }
    
    *num_processes = id_cnt;

    if (id_cnt == 0) {
	return NULL;
    }


    info_arr = calloc(sizeof(struct process_info), id_cnt);

    for (i = 0; i < id_cnt; i++) {
	info_arr[i].id         = id_arr[i];

	info_arr[i].state      = hdb_get_process_state  ( hobbes_master_db, id_arr[i] );
	info_arr[i].enclave_id = hdb_get_process_enclave( hobbes_master_db, id_arr[i] );

	strncpy(info_arr[i].name, hdb_get_process_name(hobbes_master_db, id_arr[i]), 31);
    }

    free(id_arr);
    
    return info_arr;
}



const char *
process_state_to_str(process_state_t state) 
{
    switch (state) {
	case PROCESS_INITTED: return "Initialized";
	case PROCESS_RUNNING: return "Running";
	case PROCESS_STOPPED: return "Stopped";
	case PROCESS_CRASHED: return "Crashed";
	case PROCESS_ERROR:   return "Error";

	default: return NULL;
    }

    return NULL;
}
    
