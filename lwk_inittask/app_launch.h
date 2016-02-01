/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __APP_LAUNCH_H__
#define __APP_LAUNCH_H__

#include <hobbes_app.h>


typedef union {
    uint64_t flags;
    struct {
	uint64_t use_large_pages : 1;
	uint64_t use_smartmap    : 1;
	uint64_t use_prealloc_mem: 1;
	uint64_t hobbes_enabled  : 1;
	uint64_t rsvd            : 60;
    } __attribute__((packed));
}  job_flags_t;


int launch_lwk_app(char        * name, 
		   char        * exe_path, 
		   char        * argv, 
		   char        * envp, 
		   job_flags_t   flags,
		   uint8_t       ranks, 
		   uint64_t      cpu_mask,
		   uint64_t      heap_size,
		   uint64_t      stack_size,
		   uintptr_t     data_base_addr,
		   uintptr_t     heap_base_addr,
		   uintptr_t     stack_base_addr);

int launch_hobbes_lwk_app(char * spec_str);
int kill_hobbes_lwk_app(hobbes_id_t hpid);


#endif
