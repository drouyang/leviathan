/* Kitten Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __APP_LAUNCH_H__
#define __APP_LAUNCH_H__


typedef union {
    uint64_t flags;
    struct {
	uint64_t use_large_pages : 1;
	uint64_t use_smartmap    : 1;
	uint64_t hobbes_enabled  : 1;
	uint64_t rsvd            : 61;
    } __attribute__((packed));
}  job_flags_t;


int launch_lnx_app(char        * name, 
		   char        * exe_path, 
		   char        * argv, 
		   char        * envp);

int launch_hobbes_lnx_app(char * spec_str);


#endif
