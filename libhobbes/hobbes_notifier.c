/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */



#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include <pet_log.h>

#include "hobbes.h"
#include "hobbes_db.h"
#include "hobbes_notifier.h"
#include "hobbes_util.h"
#include "xemem.h"



extern hdb_db_t hobbes_master_db;



hnotif_t *
hnotif_create(uint64_t evt_mask)
{
    hnotif_t    * notifier = NULL;
    xemem_segid_t segid;

    int fd = 0;

    if (evt_mask & HNOTIF_UNUSED_FLAGS) {
	ERROR("Could not create notifier with invalid mask\n");
	return NULL;
    }


    segid = xemem_make_signalled(NULL, 0, NULL, &fd);

    if (segid <= 0) {
	ERROR("Could not create notifier segment\n");
	return NULL;
    }

    if (hdb_create_notifier(hobbes_master_db, segid, evt_mask) == -1) {
	ERROR("Could not insert notifier into database\n");
	xemem_remove(segid);
	return NULL;
    }


    notifier = calloc(sizeof(hnotif_t), 1);
    
    if (notifier == NULL) {
	ERROR("Could not allocate notifier\n");
	hdb_delete_notifier(hobbes_master_db, segid);
	xemem_remove(segid);
	return NULL;
    }

    notifier->fd       = fd;
    notifier->segid    = segid;
    notifier->evt_mask = evt_mask;
    
    return notifier;
}


void
hnotif_free(hnotif_t  * notifier)
{

    hdb_delete_notifier(hobbes_master_db, notifier->segid);
    xemem_remove(notifier->segid);
    free(notifier);

    return;
}


int
hnotif_get_fd(hnotif_t * notifier)
{
    return notifier->fd;
}


int 
hnotif_signal(uint64_t evt_mask)
{
    xemem_segid_t * segids   = NULL;
    uint32_t        subs_cnt = 0;
    uint32_t        i        = 0;

    segids = hdb_get_event_subscribers(hobbes_master_db, evt_mask, &subs_cnt);

    if (segids == NULL) {
	ERROR("Could not retrieve subscriber list\n");
	return -1;
    }

    
    for (i = 0; i < subs_cnt; i++) {
	xemem_signal_segid(segids[i]);
    }
    
    return 0;

}
