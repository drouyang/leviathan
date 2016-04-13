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
#include <hobbes_util.h>
#include <xemem.h>

#define SIZE                    (4*1024)
#define HIO_SEG_NAME            "hio_engine_seg"

int main(int argc, char* argv[])
{
    int *buf;

    hobbes_client_init();

    xemem_segid_t segid;
    segid = xemem_lookup_segid(HIO_SEG_NAME);
    if (segid == XEMEM_INVALID_SEGID) {
        printf("xemem_lookup_segid failed\n");
        return -1;
    }

    xemem_apid_t apid = xemem_get(segid, XEMEM_RDWR);
    if (apid == -1) {
        printf("Cannot get data segid %li\n", segid);
        return -1;
    }

    struct xemem_addr addr;
    addr.apid = apid;
    addr.offset = 0;
    buf = xemem_attach(addr, SIZE, NULL);

    printf("Buffer content %x\n", *buf);

    if (xemem_detach(buf) < 0) {
        printf("xemem_detach failed\n");
    }

    if (xemem_release(apid) < 0) {
        printf("xemem_release failed\n");
    }

    return 0;
}
