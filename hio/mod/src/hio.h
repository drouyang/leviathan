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

#include <linux/cdev.h>           /* cdev_init cdev_add */

struct __attribute__((__packed__)) hio_stub {
    int stub_id;
    struct hio_engine *hio_engine;

    spinlock_t                  lock;
    struct hio_syscall_t      *pending_syscall; 
    bool                        is_pending;
    wait_queue_head_t           syscall_wq;

    dev_t           dev; 
    struct cdev     cdev;
};


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

    // We could use hashmap here, but for now just use array
    // and use rank number as the key
    struct hio_stub *stub_lookup_table[MAX_STUBS];

    struct task_struct         *handler_thread;

    // wait for syscall requests
    wait_queue_head_t           syscall_wq;
};


int hio_engine_init(struct hio_engine *hio_engine);
int hio_engine_deinit(struct hio_engine *hio_engine);
int hio_engine_event_loop(struct hio_engine *engine);
int hio_engine_add_ret(struct hio_engine *engine, struct hio_syscall_ret_t *ret);
int hio_engine_test_syscall(struct hio_engine *hio_engine, struct hio_syscall_t *syscall);
int stub_register(struct hio_engine *hio_engine, int stub_id);
int stub_deregister(struct hio_engine *hio_engine, int stub_id);
struct hio_stub * lookup_stub(struct hio_engine *hio_engine, int stub_id);
int add_stub(struct hio_engine *hio_engine, int stub_id, struct hio_stub *stub);
int remove_stub(struct hio_engine *hio_engine, int stub_id);
#endif
