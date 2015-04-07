/* Hobbes Enclave Commands 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */


#ifndef __HOBBES_CMDS_H__
#define __HOBBES_CMDS_H__

#include <hobbes_cmd_queue.h>

typedef int (*hobbes_cmd_fn)(hcq_handle_t  hcq,
                             hcq_cmd_t     cmd);



hcq_handle_t hobbes_cmd_init(void);
int hobbes_handle_cmd(hcq_handle_t hcq);
int register_hobbes_cmd(uint64_t cmd, hobbes_cmd_fn handler);







#endif
