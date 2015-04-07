
/* Hobbes utility functions  
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __HOBBES_UTIL_H__
#define __HOBBES_UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static inline int
smart_atoi(int dflt, char * str) 
{
    char * end = NULL;
    int    tmp = 0;
    
    if ((str == NULL) || (*str == '\0')) {
        /*  String was either NULL or empty */
        return dflt;
    }

    tmp = strtol(str, &end, 10);

    if (*end) {
        /* String contained non-numerics */
        return dflt;
    }
    
    return tmp;
}


#endif
