/* 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 * HIO Engine User Space Process 
 *
 * This process export a shared memory region via xemem
 * and pass the memory region to kernel
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>
#include<signal.h>
#include<unistd.h>



#include <hio_ioctl.h>
#include <pet_ioctl.h>
#include <xemem.h>
#include <hobbes_util.h>
#include <hobbes_cmd_queue.h>

xemem_segid_t segid;

static void sig_handler(int signo)
{
    if (signo == SIGINT) {
        printf("Terminaling engine is not supported!\n");
        printf("    The HIO kernel module uses memory region exported by the engine process with xemem\n");
        printf("    Terminating the engine process will destory the memory region and corrupt the hio module\n"); 
        printf("    To terminate, remove the hio module\n"); 
    }
}

int main(int argc, char* argv[])
{
    int ret;
    char hio_fname[128] = "/dev/hio";

    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        printf("Warning: fail to catch SIGINT, do not ctrl-c this process\n");
        return -1;
    }
    printf("Start engine process...\n");
    printf("Warning: DO NOT CTRL-C this process!\n");
    printf("    The HIO kernel module uses memory region exported by the engine process with xemem\n");
    printf("    Terminating the engine process will destory the memory region and corrupt the hio module\n"); 
    printf("    Currently there's no way to correctly terminate it\n"); 
    printf("    CTRL-C will result in a infinite loop in the kernel\n"); 

    ret = access(hio_fname, F_OK);
    if(ret != 0) {
        printf("Error(%d): %s does not exist\n", ret, hio_fname);
        return -1;
    }

    ret = access(hio_fname, W_OK | R_OK);
    if(ret != 0) {
        printf("Error(%d): accessing %s. Try sudo?\n", ret, hio_fname);
        return -1;
    }


    /* Allocating page aligned memory*/
    void *buf;
    posix_memalign((void **)&buf, HIO_ENGINE_PAGE_SIZE, HIO_ENGINE_PAGE_SIZE);
    if (buf == NULL) {
        printf("memory allocation failed\n");
        return -1;
    }
    memset(buf, 0, HIO_ENGINE_PAGE_SIZE);
    {
        int *ptr = buf;
        *ptr = HIO_ENGINE_MAGIC;
        printf("Buf addr %p, content %x\n", ptr, *ptr);
    }
    
    /* export memory */
    {
        printf("Exporting buf %p with XEMEM...\n", buf);
        hobbes_client_init();
        segid = xemem_make(buf, HIO_ENGINE_PAGE_SIZE, HIO_ENGINE_SEG_NAME);
        if (segid == XEMEM_INVALID_SEGID) {
            printf("xemem_make failed\n");
            free(buf);
            return -1;
        }
    }


    ret = pet_ioctl_path("/dev/hio", HIO_IOCTL_ENGINE_START , buf);
    printf("Return from kernel with ret %d\n", ret);

    xemem_remove(segid);
    return 0;
}
