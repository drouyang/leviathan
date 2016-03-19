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
#include <linux/kthread.h>

#define MAX_STUBS           1024
#define HIO_RB_SIZE             MAX_STUBS

// transferred in the ringbuffer
struct hio_cmd_t {
    int app_id;
    int syscall_nr;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
    uint64_t ret_val;
    uint64_t errno;
};


// used in ioctls
struct stub_syscall_t {
    int app_id;
    int syscall_nr;
    uint64_t arg0;
    uint64_t arg1;
    uint64_t arg2;
    uint64_t arg3;
    uint64_t arg4;
};


// used in ioctls
struct stub_syscall_ret_t {
    int app_id;
    int syscall_nr;
    int ret_val;
    int errno;
};

struct hio_stub {
    int app_id;
    struct hio_engine *hio_engine;

    spinlock_t                  lock;
    struct stub_syscall_t      *pending_syscall; 
    bool                        is_pending;
    wait_queue_head_t           syscall_wq;

    dev_t           dev; 
    struct cdev     cdev;
};


struct hio_engine {
    // We could use hashmap here, but for now just use array
    // and use rank number as the key
    struct hio_stub *stub_lookup_table[MAX_STUBS];

    spinlock_t                  lock;
    struct hio_cmd_t            rb[HIO_RB_SIZE];
    int                         rb_syscall_prod_idx;     // shared, updated by hio client
    int                         rb_ret_cons_idx;         // client private
    int                         rb_syscall_cons_idx;     // engine private
    int                         rb_ret_prod_idx;         // shared, updated by hio engine

    struct task_struct         *handler_thread;
};


int hio_engine_init(struct hio_engine *hio_engine);
int stub_register(struct hio_engine *hio_engine, int app_id);
int stub_deregister(struct hio_engine *hio_engine, int app_id);
struct hio_stub * lookup_stub(struct hio_engine *hio_engine, int app_id);
int insert_stub(struct hio_engine *hio_engine, int key, struct hio_stub *stub);
int remove_stub(struct hio_engine *hio_engine, int key);
#endif