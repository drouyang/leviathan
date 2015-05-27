/* Leviathan Linux inittask 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __INIT_H__
#define __INIT_H__


#include <stdint.h>

extern uint64_t   enclave_id;
extern char     * enclave_name;




typedef int (*fd_handler_fn)(int fd, void * priv_data);


int add_fd_handler(int             fd, 
		   fd_handler_fn   handler,
		   void          * priv_data);

int remove_fd_handler(int fd);


#endif
