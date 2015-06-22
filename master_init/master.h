/* Master control process 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __MASTER_H__
#define __MASTER_H__


typedef int (*fd_handler_fn)(int fd, void * priv_data);


int remove_fd_handler(int fd);
int add_fd_handler(int fd, fd_handler_fn fn, void * priv_data);


#endif
