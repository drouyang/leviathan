/* Hobbes Runtime Library
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __HOBBES_H__
#define __HOBBES_H__


#include <stdbool.h>
#include <stdint.h>


#define HOBBES_ENV_ENCLAVE_ID "HOBBES_ENCLAVE_ID"
#define HOBBES_ENV_PROCESS_ID "HOBBES_PROCESS_ID"

#define HOBBES_INVALID_ID (-1)

typedef int hobbes_id_t;


bool hobbes_is_enabled( void );

char *      hobbes_get_my_enclave_name( void );
char *      hobbes_get_my_process_name( void );
hobbes_id_t hobbes_get_my_enclave_id( void );
hobbes_id_t hobbes_get_my_process_id( void );

int hobbes_client_init( void );
int hobbes_client_deinit( void );


#endif
