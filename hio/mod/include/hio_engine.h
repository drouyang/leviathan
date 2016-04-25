/* 
 * HIO Header
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_ENGINE_H_
#define _HIO_ENGINE_H_

#define MAX_STUBS               32
#define HIO_RB_SIZE             MAX_STUBS

#include "hio_ioctl.h"
#include "pisces_lock.h"

// transferred in the ringbuffer
struct __attribute__((__packed__)) hio_cmd_t {
    int stub_id;
    int syscall_nr;
    unsigned long long arg0;
    unsigned long long arg1;
    unsigned long long arg2;
    unsigned long long arg3;
    unsigned long long arg4;
    int ret_val;
    int ret_errno;
};


struct __attribute__((__packed__)) hio_engine {
    unsigned int                magic;
    int                         rb_syscall_prod_idx;     // shared, updated by hio client
    int                         rb_ret_cons_idx;         // client private
    int                         rb_syscall_cons_idx;     // engine private
    int                         rb_ret_prod_idx;         // shared, updated by hio engine
    struct hio_cmd_t            rb[HIO_RB_SIZE];
    struct pisces_spinlock      lock;
};

#endif
