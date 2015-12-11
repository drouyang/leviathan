/* Palacios control functions
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __PALACIOS_H__
#define __PALACIOS_H__

#include <stdbool.h>


extern bool palacios_enabled;

int  palacios_init(void);

bool palacios_is_available(void);

int ensure_valid_host_memory(void);


#endif
