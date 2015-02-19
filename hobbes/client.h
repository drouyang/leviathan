/* Hobbes client program interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "hobbes_db.h"
#include "enclave.h"
#include <xpmem.h>

#define HOBBES_MAX_SEGMENT_NAME_LEN 64

struct hobbes_segment {
    xpmem_segid_t segid;
    char          name[HOBBES_MAX_SEGMENT_NAME_LEN];
};



extern hdb_db_t hobbes_master_db;

int hobbes_client_init();
int hobbes_client_deinit();

int hobbes_client_export_segment(xpmem_segid_t segid, char * name);
int hobbes_client_remove_segment(xpmem_segid_t segid);

struct hobbes_segment * client_get_segment_list();

#endif
