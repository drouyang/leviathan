
#ifndef __PISCES_H__
#define __PISCES_H__

#include <stdlib.h>


size_t pisces_file_stat(char * filename);
int pisces_file_load(char * filename, void * addr);



int pisces_init();


#endif
