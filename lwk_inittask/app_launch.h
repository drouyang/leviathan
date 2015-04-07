/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __JOB_LAUNCH_H__
#define __JOB_LAUNCH_H__


typedef union {
    uint64_t flags;
    struct {
	uint64_t use_large_pages : 1;
	uint64_t use_smartmap    : 1;
	uint64_t hobbes_enabled  : 1;
	uint64_t rsvd            : 61;
    } __attribute__((packed));
}  job_flags_t;


int launch_app(char        * name, 
	       char        * exe_path, 
	       char        * argv, 
	       char        * envp, 
	       job_flags_t   flags,
	       uint8_t       ranks, 
	       uint64_t      cpu_mask,
	       uint64_t      heap_size,
	       uint64_t      stack_size);




#endif
