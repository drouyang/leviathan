/* Hobbes enclave Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __ENCLAVE_H__
#define __ENCLAVE_H__

#include "hobbes_types.h"




typedef enum {
    INVALID_ENCLAVE   = 0,
    MASTER_ENCLAVE    = 1,
    PISCES_ENCLAVE    = 2,
    PISCES_VM_ENCLAVE = 3,
    LINUX_VM_ENCLAVE  = 4
} enclave_type_t;

struct hobbes_enclave {
    char name[32];

    u64 enclave_id;
    u64 parent_id;

    int mgmt_dev_id;
    
    enclave_type_t type;
};


int create_enclave(char * cfg_file_name, char * name);
int destroy_enclave(char * enclave_name);

const char * 
enclave_type_to_str(enclave_type_t type);


#endif
