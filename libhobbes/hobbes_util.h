/* 
 * Copyright (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "PETLAB_LICENSE".
 */

#ifndef __HOBBES_UTIL_H__
#define __HOBBES_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <unistd.h>

#define smart_free(ptr) {free(ptr); ptr = NULL;}

uint32_t smart_atou32(uint32_t dflt, char * str);
int32_t  smart_atoi32(int32_t  dflt, char * str);
uint64_t smart_atou64(uint64_t dflt, char * str);
int64_t  smart_atoi64(int64_t  dflt, char * str);


#define smart_atoi smart_atoi32

static inline int
smart_strlen(char * str) 
{
    if (str == NULL) {
	return -1;
    }

    return strlen(str);
}


#ifdef __cplusplus
}
#endif

#endif
