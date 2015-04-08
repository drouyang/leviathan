/* Hobbes Enclave Command handler 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __LWK_HOBBES_CMDS_H__
#define __LWK_HOBBES_CMDS_H__

#include <hobbes_cmd_queue.h>
#include <hobbes_cmds.h>

typedef int (*hobbes_cmd_fn)(hcq_handle_t  hcq,
                             hcq_cmd_t     cmd);



hcq_handle_t hobbes_cmd_init(void);
int hobbes_handle_cmd(hcq_handle_t hcq);
int register_hobbes_cmd(uint64_t cmd, hobbes_cmd_fn handler);







#endif
