/* HIO Interface Library 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int ret;
    char stub_fname[128];

    sprintf(stub_fname, "/dev/hio-stub%d", STUB_ID);

    printf("getpid returns %d\n", getpid());

    struct stub_syscall_t syscall_ioclt;
    memset(&syscall_ioclt, 0, sizeof(struct stub_syscall_t));
    syscall_ioclt.syscall_nr = GETPID;

    printf("issue syscall %d (%llu, %llu, %llu, %llu, %llu)\n",
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

    printf("syscall returns %d\n", ret);

    ret = access(stub_fname, F_OK);
    if(ret != 0) {
        printf("Error: %s does not exist\n", stub_fname);
        return -1;
    }

    ret = access(stub_fname, R_OK | W_OK);
    if(ret != 0) {
        printf("Error: cannot read or write %s, try sudo\n", stub_fname);
        return -1;
    }

    ret = pet_ioctl_path(stub_fname, HIO_STUB_SYSCALL, (void *) &syscall_ioclt);
    printf("syscall_ioctl ret %d\n", ret);

    return 0;
}
