/* Pisces LWK inittask 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __INIT_H__
#define __INIT_H__

#include "cmds.h"

extern cpu_set_t enclave_cpus;
extern uint64_t  enclave_id;

typedef int (*cmd_handler_fn)(hcq_handle_t  hcq,
			      hcq_cmd_t     cmd, 
			      void        * priv_data);

typedef int (*legacy_cmd_handler_fn)(int pisces_fd,
				     uint64_t cmd);



int register_cmd_handler(cmd_handler_fn handler, void * priv_data);


#endif
