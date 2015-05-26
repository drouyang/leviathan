/* Linux Job Launch 
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __APP_LAUNCH_H__
#define __APP_LAUNCH_H__

#include <pet_list.h>






struct app_state {
    struct pet_list node;

    hobbes_id_t hpid;

    pid_t pid;

    FILE * pipe_fp;
};



int launch_lnx_app(char * name, 
		   char * exe_path, 
		   char * argv, 
		   char * envp);

int launch_hobbes_lnx_app(char * spec_str);


#endif
