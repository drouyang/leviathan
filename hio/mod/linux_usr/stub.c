/* HIO Interface Library 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/syscall.h>

#include <hio_ioctl.h>
#include <pet_ioctl.h>

#define STUB_ID     123
#define GETPID      39

int main(int argc, char* argv[])
{
    printf("Start stub process...\n");

    int fd = pet_ioctl_path("/dev/hio", HIO_IOCTL_REGISTER , (void *) STUB_ID);
    printf("fd: %d\n", fd);

    return 0;

    while (1) {
        struct stub_syscall_t syscall_ioclt;
        char fname[128];
        sprintf(fname, "/dev/hio-stub%d", STUB_ID);

        printf("poll file %s\n", fname);

        int ret = pet_ioctl_path(fname, HIO_STUB_SYSCALL_POLL, (void *) &syscall_ioclt);
        printf("stub_id %d get syscall_ioclt: %d (%llu, %llu, %llu, %llu, %llu)\n",
                syscall_ioclt.stub_id,
                syscall_ioclt.syscall_nr,
                syscall_ioclt.arg0,
                syscall_ioclt.arg1,
                syscall_ioclt.arg2,
                syscall_ioclt.arg3,
                syscall_ioclt.arg4);

        ret = syscall(syscall_ioclt.syscall_nr, 
                syscall_ioclt.arg0,
                syscall_ioclt.arg1, 
                syscall_ioclt.arg2, 
                syscall_ioclt.arg3, 
                syscall_ioclt.arg4);

        printf("syscall_ioctl ret %d\n", ret);
    }

    return 0;
}
