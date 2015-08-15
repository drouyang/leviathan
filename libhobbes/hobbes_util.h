
/* Hobbes utility functions  
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_UTIL_H__
#define __HOBBES_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>

#define smart_free(ptr) {free(ptr); ptr = NULL;}

int      smart_atoi  (int      dflt, char * str);
uint64_t smart_atou64(uint64_t dflt, char * str);
int64_t  smart_atoi64(int64_t  dflt, char * str);


static inline int
smart_strlen(char * str) 
{
    if (str == NULL) {
	return -1;
    }

    return strlen(str);
}


#endif
