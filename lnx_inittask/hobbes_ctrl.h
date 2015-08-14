/* Hobbes Enclave Command handler 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __LWK_HOBBES_CMDS_H__
#define __LWK_HOBBES_CMDS_H__

#include <hobbes_cmd_queue.h>
#include <hobbes_cmds.h>


extern bool hobbes_enabled;

typedef int (*hobbes_cmd_fn)(hcq_handle_t  hcq,
                             hcq_cmd_t     cmd);



int hobbes_init( void );

int hobbes_register_cmd(uint64_t cmd, hobbes_cmd_fn handler);







#endif
