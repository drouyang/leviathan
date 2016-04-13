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

#include <hio_ioctl.h>
#include <pet_ioctl.h>
#include <xemem.h>
#include <hobbes_util.h>
#include <hobbes_cmd_queue.h>

#define SIZE                    (4*1024)
#define HIO_SEG_NAME            "hio_engine_seg"

xemem_segid_t segid;

int main(int argc, char* argv[])
{
    int ret;
    char hio_fname[128] = "/dev/hio";

    printf("Start engine process...\n");

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
    posix_memalign((void **)&buf, SIZE, SIZE);
    if (buf == NULL) {
        printf("memory allocation failed\n");
        return -1;
    }
    memset(buf, 0, SIZE);
    {
        int *ptr = buf;
        *ptr = HIO_ENGINE_MAGIC;
        printf("Buf addr %p, content %x\n", ptr, *ptr);
    }
    
    /* export memory */
    {
        printf("Exporting buf %p with XEMEM...\n", buf);
        hobbes_client_init();
        segid = xemem_make(buf, SIZE, HIO_SEG_NAME);
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
