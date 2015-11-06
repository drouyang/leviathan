/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __CMD_QUEUE_H__
#define __CMD_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>

#include "xemem.h"
#include "hobbes_cmds.h"

#define HCQ_INVALID_CMD    ((uint64_t)-1)
#define HCQ_INVALID_HANDLE (NULL)

typedef enum {
    HCQ_CMD_PENDING  = 0,
    HCQ_CMD_RETURNED = 1} hcq_cmd_status_t;


typedef void *   hcq_handle_t;
typedef uint64_t hcq_cmd_t;

hcq_handle_t hcq_create_queue(char * name);
void hcq_free_queue(hcq_handle_t hcq);

xemem_segid_t hcq_get_segid(hcq_handle_t hcq);
int hcq_get_fd(hcq_handle_t hcq);


hcq_handle_t hcq_connect(xemem_segid_t segid);
void hcq_disconnect(hcq_handle_t hcq);


void hcq_dump_queue(hcq_handle_t hcq);

hcq_cmd_t hcq_cmd_issue(hcq_handle_t hcq, 
			uint64_t     cmd_code,
			uint32_t     data_size,
			void       * data);



hcq_cmd_status_t hcq_get_cmd_status(hcq_handle_t hcq, 
				    hcq_cmd_t    cmd);

int64_t hcq_get_ret_code(hcq_handle_t hcq, 
			 hcq_cmd_t    cmd);

void * hcq_get_ret_data(hcq_handle_t hcq, 
			hcq_cmd_t    cmd,
			uint32_t   * size);

int hcq_cmd_complete(hcq_handle_t hcq, 
		     hcq_cmd_t    cmd);




hcq_cmd_t hcq_get_next_cmd(hcq_handle_t hcq);


uint64_t hcq_get_cmd_code(hcq_handle_t hcq, 
			  hcq_cmd_t    cmd);

void * hcq_get_cmd_data(hcq_handle_t   hcq, 
			hcq_cmd_t      cmd,
			uint32_t     * size);


int hcq_cmd_return(hcq_handle_t hcq, 
		   hcq_cmd_t    cmd, 
		   int64_t      ret_code, 
		   uint32_t     data_size,
		   void       * data);




#ifdef __cplusplus
}
#endif

#endif
