/* Pisces LWK inittask 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __INIT_H__
#define __INIT_H__


#include <lwk/liblwk.h>
#include <lwk/smp.h>
#include <sys/types.h>
#include <pthread.h>

#include <hobbes_cmd_queue.h>

extern cpu_set_t  enclave_cpus;
extern uint64_t   enclave_id;
extern char     * enclave_name;

extern bool palacios_enabled;
extern bool hobbes_enabled;


int  load_remote_file(char * remote_file, char * local_file);
int store_remote_file(char * remote_file, char * local_file);

#endif
