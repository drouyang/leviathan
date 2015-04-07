/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_ENCLAVE_H__
#define __HOBBES_ENCLAVE_H__

#include <stdint.h>

#include "hobbes.h"
#include "hobbes_cmd_queue.h"

typedef enum {
    ENCLAVE_INITTED   = 0,
    ENCLAVE_RUNNING   = 1,
    ENCLAVE_STOPPED   = 2,
    ENCLAVE_CRASHED   = 3,
    ENCLAVE_ERROR     = 4
} enclave_state_t;

typedef enum {
    INVALID_ENCLAVE   = 0,
    MASTER_ENCLAVE    = 1,
    PISCES_ENCLAVE    = 2,
    PISCES_VM_ENCLAVE = 3,
    LINUX_VM_ENCLAVE  = 4
} enclave_type_t;



int hobbes_create_enclave(char * cfg_file_name, char * name);
int hobbes_destroy_enclave(hobbes_id_t enclave_id);


hobbes_id_t hobbes_get_enclave_id(char * enclave_name);
char *      hobbes_get_enclave_name(hobbes_id_t enclave_id);

hcq_handle_t hobbes_open_enclave_cmdq(hobbes_id_t enclave_id);
int hobbes_register_enclave_cmdq(hobbes_id_t enclave_id, xemem_segid_t segid);

struct enclave_info {
    hobbes_id_t id;

    char name[32];

    enclave_type_t  type;
    enclave_state_t state;
};

struct enclave_info * 
hobbes_get_enclave_list(int * num_enclaves);



const char * 
enclave_type_to_str(enclave_type_t type);

const char *
enclave_state_to_str(enclave_state_t state);


#endif
