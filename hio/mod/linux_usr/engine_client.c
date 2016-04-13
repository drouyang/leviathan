/* 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 * HIO Engine User Space CLIENT Process (Kitten)
 *
 * This process attach to a shared memory region via xemem
 * and pass the memory region to the kitten kernel
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
#include <hobbes_util.h>
#include <xemem.h>

int main(int argc, char* argv[])
{
    int *buf;

    hobbes_client_init();

    xemem_segid_t segid;
    segid = xemem_lookup_segid(HIO_ENGINE_SEG_NAME);
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
    buf = xemem_attach(addr, HIO_ENGINE_PAGE_SIZE, NULL);

    printf("Buffer address %p, buffer content %x\n", buf, *buf);
    printf("Enter kernel...\n");

    {
        int ret = pet_ioctl_path("/dev/hio", HIO_IOCTL_ENGINE_ATTACH , buf);
        printf("Return from kernel with ret %d\n", ret);
    }

    if (xemem_detach(buf) < 0) {
        printf("xemem_detach failed\n");
    }

    if (xemem_release(apid) < 0) {
        printf("xemem_release failed\n");
    }

    return 0;
}
