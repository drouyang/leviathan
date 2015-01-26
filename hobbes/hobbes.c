/* Hobbes Management interface
 * (c) 2015, Jack Lange <jacklange@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>

#include "enclave.h"

int 
main(int argc, char ** argv) 
{


    create_enclave(argv[1]);

    return 0;
}
