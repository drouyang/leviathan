#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include <libhio.h>

int 
main(int     argc,
     char ** argv)
{
    int status;

    status = libhio_init(&argc, &argv);
    if (status != 0)
        return -1;

    /* A real stub would probably setup a command queue or a signallable
     * segment and wait for requests from the Kitten side.  
     */

    sleep(2);

    /* These are deterministic addresses where Kitten's elf loader maps
     * the test app's (../app) bss, stack, and heap. The app initializes them
     * to {0,1,2} + (10 * PMI_RANK), respectively. So, rank 0 initiales these
     * to 0,1,2, rank 1 to 10,11,12, etc. If the initialization was successful
     * we should see these values printed here.
     *
     * A real stub would probably setup up a command queue or signallable
     * segment of some sort to handle requests. We just print these, sleep for
     * a bit, then exit.
     */
    {
        int * global_addr = (int *)0x6ba790;
        int * stack_addr  = (int *)0x7ffffffd60;
        int * malloc_addr = (int *)0x6bf870;

        fprintf(stderr, "global_int: %d\n", *global_addr);
        fprintf(stderr, "stack_int:  %d\n", *stack_addr);
        fprintf(stderr, "malloc_int: %d\n", *malloc_addr);
    }


    sleep(10);

    libhio_deinit();

    return 0;
} 
