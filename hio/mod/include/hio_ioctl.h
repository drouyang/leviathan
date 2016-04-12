/* 
 * HIO IOCTL Header
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_IOCTL_H_
#define _HIO_IOCTL_H_

// hio commands
#define HIO_IOCTL_ENGINE_START      3300
#define HIO_IOCTL_REGISTER          3301 // register stub
#define HIO_IOCTL_DEREGISTER        3302

// stub commands
#define HIO_STUB_SYSCALL_POLL       3400
#define HIO_STUB_SYSCALL_RET        3401
#define HIO_STUB_TEST_SYSCALL       3402


// used in ioctls
struct stub_syscall_t {
    int stub_id;
    int syscall_nr;
    unsigned long long arg0;
    unsigned long long arg1;
    unsigned long long arg2;
    unsigned long long arg3;
    unsigned long long arg4;
};


// used in ioctls
struct stub_syscall_ret_t {
    int stub_id;
    int syscall_nr;
    int ret_val;
    int ret_errno;
};


#endif
