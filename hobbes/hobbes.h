/* Hobbes Management interface
 * (c) 2014, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_H__
#define __HOBBES_H__

#include "hobbes_db.h"

typedef enum {
    INVALID_ENCLAVE    = 0,
    LINUX              = 1,
    KITTEN             = 2,
    PALACIOS_ON_LINUX  = 3,
    PALACIOS_ON_KITTEN = 4
} enclave_type_t;

struct hobbes_enclave {
    char name[32];
    uint64_t enclave_id;

    enclave_type_t type;


};



#endif
