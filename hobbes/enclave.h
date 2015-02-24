/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __ENCLAVE_H__
#define __ENCLAVE_H__

#include <stdint.h>

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

struct hobbes_enclave {
    char name[32];

    int enclave_id;
    int parent_id;

    int mgmt_dev_id;
    
    enclave_state_t state;
    enclave_type_t  type;
};


int create_enclave(char * cfg_file_name, char * name);
int destroy_enclave(char * enclave_name);

const char * 
enclave_type_to_str(enclave_type_t type);

const char *
enclave_state_to_str(enclave_state_t state);

#endif
