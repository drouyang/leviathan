/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */


#ifndef __XEMEM_H__
#define __XEMEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#include "hobbes.h"

#define XEMEM_SEG_NAME_LEN 64


typedef int64_t xemem_segid_t;
typedef int64_t xemem_apid_t;

#define XEMEM_INVALID_SEGID ((xemem_segid_t)-1)

/*
 * flags for segment permissions
 */
#define XEMEM_RDONLY    0x1
#define XEMEM_RDWR      0x2

/*
 * Valid permit_type values for xpmem_make()/xpmem_get().
 */
#define XEMEM_PERMIT_MODE       0x1


struct xemem_segment {
    xemem_segid_t segid;
    char          name[XEMEM_SEG_NAME_LEN];
    hobbes_id_t   enclave_id;
    hobbes_id_t   app_id;
};


xemem_segid_t 
xemem_make(void   * vaddr, 
	   size_t   size,
	   char   * name);


xemem_segid_t 
xemem_make_signalled(void   * vaddr, 
		     size_t   size,
		     char   * name, 
		     int    * fd);

int 
xemem_make_segid(void          * vaddr, 
		 size_t          size,
		 char          * name,
		 xemem_segid_t   request);


int xemem_remove(xemem_segid_t segid);



xemem_apid_t xemem_get(xemem_segid_t  segid,
		       int            flags);


int xemem_release(xemem_apid_t apid);

int xemem_signal(xemem_apid_t apid);
int xemem_signal_segid(xemem_segid_t segid);

int xemem_ack(int fd);
int xemem_ack_all(int fd);

struct xemem_addr {
    xemem_apid_t apid;    /* apid that represents memory */
    off_t        offset;  /* offset into apid's memory */
};

void * xemem_attach(struct xemem_addr addr, 
		    size_t            size,
		    void            * vaddr);

void * xemem_attach_nocache(struct xemem_addr addr, 
		    size_t            size,
		    void            * vaddr);

int xemem_detach(void * vaddr);




xemem_segid_t xemem_lookup_segid(char * name);


struct xemem_segment * 
xemem_get_segment_list(int * num_segments);


#ifdef __cplusplus
}
#endif

#endif
