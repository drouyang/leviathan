/* 
 * HIO Header
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#ifndef _HIO_H_
#define _HIO_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>             /* device file */
#include <linux/types.h>          /* dev_t */
#include <linux/kdev_t.h>         /* MAJOR MINOR MKDEV */
#include <linux/device.h>         /* udev */
#include <linux/cdev.h>           /* cdev_init cdev_add */
#include <linux/moduleparam.h>    /* module_param */
#include <linux/stat.h>           /* perms */
#include <linux/spinlock.h>
#include <linux/sched.h>

#define MAX_STUBS 1024

struct hio_syscall_cmd {
    int app_id;
    int syscall_nr;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t arg5;
};

struct hio_syscall_ret {
    int app_id;
    int syscall_nr;
    int ret_val;
    int ret_errno;
};


struct hio_stub {
    int app_id;
    struct hio_engine *hio_engine;

    spinlock_t                  syscall_ret_lock;
    struct hio_syscall_cmd      syscall_ret; 
    wait_queue_head_t           syscall_ret_waitq;

    dev_t           dev; 
    struct cdev     cdev;
};

struct hio_engine {
    // We could use hashmap here, but for now just use array
    // and use rank number as the key
    struct hio_stub *stub_lookup_table[MAX_STUBS];

    spinlock_t                  syscall_rb_lock;
    struct hio_syscall_cmd      syscall_rb[MAX_STUBS];
    int                         rb_syscall_produce_idx;     // shared, updated by hio client
    int                         rb_ret_consume_idx;         // client private
    int                         rb_syscall_consume_idx;     // engine private
    int                         rb_ret_produce_idx;         // shared, updated by hio engine
};


int hio_engine_init(struct hio_engine *hio_engine);
int stub_register(struct hio_engine *hio_engine, int app_id);
int stub_deregister(struct hio_engine *hio_engine, int app_id);
struct hio_stub * lookup_stub(struct hio_engine *hio_engine, int app_id);
int insert_stub(struct hio_engine *hio_engine, int key, struct hio_stub *stub);
int remove_stub(struct hio_engine *hio_engine, int key);
#endif
