/* Linux application control
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __LNX_APP_H__
#define __LNX_APP_H__

#include <unistd.h>

#include <pet_list.h>

#include <hobbes.h>



struct app_state {
    struct list_head node;

    hobbes_id_t hpid;
    pid_t       pid;
    int         stdin_fd;
    int         stdout_fd;
};




struct app_state *
 launch_lnx_app(char * name, 
		char * exe_path, 
		char * argv, 
		char * envp);

int launch_hobbes_lnx_app(char * spec_str);


#endif
