/* Hobbes client program interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <stdlib.h>
#include <stdio.h>


#include "hobbes_db.h"
#include "enclave.h"
#include "hobbes_types.h"


extern hdb_db_t hobbes_master_db;

int hobbes_client_init();
int hobbes_client_deinit();



#endif
