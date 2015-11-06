/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_H__
#define __HOBBES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>


#define HOBBES_ENV_ENCLAVE_ID "HOBBES_ENCLAVE_ID"
#define HOBBES_ENV_APP_ID     "HOBBES_APP_ID"

#define HOBBES_INVALID_ID (-1)

#define HOBBES_MASTER_ENCLAVE_ID (0)
#define HOBBES_INIT_APP_ID       (0)

typedef int hobbes_id_t;


bool hobbes_is_available( void );
bool hobbes_is_enabled( void );

bool hobbes_is_master_inittask( void );
bool hobbes_is_client_inittask( void );
bool hobbes_is_inittask( void );
bool hobbes_is_app( void );

char *       hobbes_get_my_enclave_name( void );
char *       hobbes_get_my_app_name( void );
hobbes_id_t  hobbes_get_my_enclave_id( void );
hobbes_id_t  hobbes_get_my_app_id( void );

int hobbes_client_init( void );
int hobbes_client_deinit( void );


#ifdef __cplusplus
}
#endif

#endif
