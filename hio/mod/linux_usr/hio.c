/* HIO Interface Library 
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include <hio_ioctl.h>
#include <pet_ioctl.h>

int main(int argc, char* argv[])
{
    int fd = pet_ioctl_path("/dev/hio", HIO_IOCTL_REGISTER , (void *) 123);
    printf("fd: %d\n", fd);
    return 0;
}
