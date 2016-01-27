/* Linux application control
 * (c) 2015, Jack Lange, <jacklange@cs.pitt.edu>
 */

#ifndef __LNX_APP_H__
#define __LNX_APP_H__

#include <unistd.h>

int launch_hobbes_lnx_app(char * spec_str);
int init_lnx_app(void);
int deinit_lnx_app(void);


#endif
