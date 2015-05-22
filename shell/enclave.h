/* Enclave control functions 
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#ifndef __ENCLAVE_H__
#define __ENCLAVE_H__

#include <hobbes.h>

int  create_enclave_main(int argc, char ** argv);
int destroy_enclave_main(int argc, char ** argv);
int    ping_enclave_main(int argc, char ** argv);
int   list_enclaves_main(int argc, char ** argv);

#endif
