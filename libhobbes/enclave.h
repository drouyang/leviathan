/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __ENCLAVE_H__
#define __ENCLAVE_H__

#include <stdint.h>

#include "xemem.h"
#include "cmd_queue.h"

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





int create_enclave(char * cfg_file_name, char * name);
int destroy_enclave(char * enclave_name);

hcq_handle_t enclave_open_cmd_queue(char * enclave_name);
int enclave_register_cmd_queue(char * enclave_name, xemem_segid_t segid);

struct enclave_info {
    int id;

    char name[32];

    enclave_type_t  type;
    enclave_state_t state;
};

struct enclave_info * 
get_enclave_list(int * num_enclaves);



const char * 
enclave_type_to_str(enclave_type_t type);

const char *
enclave_state_to_str(enclave_state_t state);


#endif
