/* 
 * HIO Engine
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 *
 * The HIO engine is a shared ringbuffer between the I/O domain and the host.
 * A kthread is created on each side to poll the pending requests.
 * 
 * On the I/O domain side, syscall requests are dispatched to hio_stubs based on 
 * the stub_id associated with each request. Currently, each hio_stub can have 
 * one pending syscall at a time.
 */
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#include "hio.h"
#include "hio_ioctl.h"

#if 0
static int hio_handler_worker(void *arg) {
    struct hio_engine *engine = (struct hio_engine *)arg;

    do {
        printk(KERN_INFO "HIO: hio_engine kthread wait...\n");
        wait_event_interruptible(engine->syscall_wq, 
            (engine->rb_syscall_prod_idx != engine->rb_syscall_cons_idx)
            || kthread_should_stop());

        printk(KERN_INFO "HIO: hio_engine kthread wakeup...\n");

        if (kthread_should_stop()) break;

        // there are pending syscalls
        while (engine->rb_syscall_prod_idx != engine->rb_syscall_cons_idx) {
            struct hio_cmd_t *cmd = &(engine->rb[engine->rb_syscall_cons_idx]);
            struct hio_stub *stub = lookup_stub(engine, cmd->stub_id);
            struct stub_syscall_t *syscall = kmalloc(sizeof(struct stub_syscall_t), GFP_KERNEL);

            if (syscall == NULL) {
                printk(KERN_ERR "Failed allocate syscall memeory\n");
                goto out;
            }

            printk(KERN_INFO "HIO ENGINE: syscall consume index %d (prod index %d)\n", 
                    engine->rb_syscall_cons_idx,
                    engine->rb_syscall_prod_idx);

            if (stub == NULL) {
                printk(KERN_ERR "stub_id %d does not exist\n", cmd->stub_id);
                goto out;
            } else if (stub->is_pending) {
                printk(KERN_ERR "Failed to process syscall while previous one is pending\n");
                printk(KERN_ERR "HIO currently supports ONE outstanding syscall per proc\n");
                goto out;
            } else {
                syscall->stub_id = cmd->stub_id;
                syscall->syscall_nr = cmd->syscall_nr;
                syscall->arg0 = cmd->arg0;
                syscall->arg1 = cmd->arg1;
                syscall->arg2 = cmd->arg2;
                syscall->arg3 = cmd->arg3;
                syscall->arg4 = cmd->arg4;

                printk(KERN_INFO "HIO: hio_engine dispatch syscall %d to stub %d\n",
                        syscall->syscall_nr,
                        syscall->stub_id);

                spin_lock(&stub->lock);
                stub->pending_syscall = syscall;
                stub->is_pending = true;
                spin_unlock(&stub->lock);
            }

            spin_lock(&engine->lock);
            engine->rb_syscall_cons_idx = (engine->rb_syscall_cons_idx + 1) % HIO_RB_SIZE;
            spin_unlock(&engine->lock);

            wake_up_interruptible(&stub->syscall_wq);
        } 
    } while (!kthread_should_stop());

out:
    while (!kthread_should_stop()) schedule();
    printk(KERN_INFO "HIO: kthread stopped\n");

    return 0;
}
#endif

int hio_engine_event_loop(struct hio_engine *engine) {
    do {
        printk(KERN_INFO "HIO: engine kthread wait...\n");
        wait_event_interruptible(engine->syscall_wq, 
            (engine->rb_syscall_prod_idx != engine->rb_syscall_cons_idx));

        printk(KERN_INFO "HIO: engine kthread wakeup...\n");

        // there are pending syscalls
        while (engine->rb_syscall_prod_idx != engine->rb_syscall_cons_idx) {
            struct hio_cmd_t *cmd = &(engine->rb[engine->rb_syscall_cons_idx]);
            struct hio_stub *stub = lookup_stub(engine, cmd->stub_id);
            struct stub_syscall_t *syscall = kmalloc(sizeof(struct stub_syscall_t), GFP_KERNEL);

            if (syscall == NULL) {
                printk(KERN_ERR "Failed allocate syscall memeory\n");
                goto out;
            }

            printk(KERN_INFO "HIO ENGINE: syscall consume index %d (prod index %d)\n", 
                    engine->rb_syscall_cons_idx,
                    engine->rb_syscall_prod_idx);

            if (stub == NULL) {
                printk(KERN_ERR "stub_id %d does not exist\n", cmd->stub_id);
                goto out;
            } else if (stub->is_pending) {
                printk(KERN_ERR "Failed to process syscall while previous one is pending\n");
                printk(KERN_ERR "HIO currently supports ONE outstanding syscall per proc\n");
                goto out;
            } else {
                syscall->stub_id = cmd->stub_id;
                syscall->syscall_nr = cmd->syscall_nr;
                syscall->arg0 = cmd->arg0;
                syscall->arg1 = cmd->arg1;
                syscall->arg2 = cmd->arg2;
                syscall->arg3 = cmd->arg3;
                syscall->arg4 = cmd->arg4;

                printk(KERN_INFO "HIO: engine dispatch syscall %d to stub %d\n",
                        syscall->syscall_nr,
                        syscall->stub_id);

                spin_lock(&stub->lock);
                stub->pending_syscall = syscall;
                stub->is_pending = true;
                spin_unlock(&stub->lock);
            }

            spin_lock(&engine->lock);
            engine->rb_syscall_cons_idx = (engine->rb_syscall_cons_idx + 1) % HIO_RB_SIZE;
            spin_unlock(&engine->lock);

            wake_up_interruptible(&stub->syscall_wq);
        } 
        schedule();
    } while (1);

out:
    while (!kthread_should_stop()) schedule();
    printk(KERN_INFO "HIO: kthread stopped\n");
    return 0;
}

void hio_engine_add_syscall(struct hio_engine *engine, struct stub_syscall_t *syscall) {
    // Insert syscall into hio_engine
    spin_lock(&engine->lock);
    if ((engine->rb_syscall_prod_idx + 1) % HIO_RB_SIZE != engine->rb_ret_cons_idx) {
        // ringbuffer is not full
        struct hio_cmd_t *cmd = &(engine->rb[engine->rb_syscall_prod_idx]);
        printk(KERN_INFO "HIO ENGINE: insert syscall at rb index %d\n", engine->rb_syscall_prod_idx);
        cmd->stub_id    =   syscall->stub_id;
        cmd->syscall_nr =   syscall->syscall_nr;
        cmd->arg0       =   syscall->arg0; 
        cmd->arg1       =   syscall->arg1;
        cmd->arg2       =   syscall->arg2;
        cmd->arg3       =   syscall->arg3;
        cmd->arg4       =   syscall->arg4;

        printk(KERN_INFO "HIO ENGINE: rb index %d, stub_id %d\n", engine->rb_syscall_prod_idx, cmd->stub_id);

        engine->rb_syscall_prod_idx = (engine->rb_syscall_prod_idx + 1) % HIO_RB_SIZE;
    } else {
        printk(KERN_ERR "HIO ENGINE: ring buffer is full!!!\n");
    }
    spin_unlock(&engine->lock);
   
    wake_up(&engine->syscall_wq);
    kfree(syscall);
}

int hio_engine_add_ret(struct hio_engine *engine, struct stub_syscall_ret_t *ret) {
    struct hio_cmd_t *hio_cmd;

    spin_lock(&engine->lock);
    if (engine->rb_ret_prod_idx == engine->rb_syscall_cons_idx) {
        printk(KERN_ERR "No pending syscall needs a return\n");
        printk(KERN_ERR "Return before handling a syscall???\n");
        return -1;
    }
    hio_cmd = &engine->rb[engine->rb_ret_prod_idx];
    hio_cmd->ret_val = ret->ret_val;
    hio_cmd->errno = ret->ret_errno;
    engine->rb_ret_prod_idx = (engine->rb_ret_prod_idx+1) % HIO_RB_SIZE;
    spin_unlock(&engine->lock);
    return 0;
}


// this is for test purpose
int hio_engine_test_syscall(struct hio_engine *engine, struct stub_syscall_t *syscall) {
    int ret = 0;

    hio_engine_add_syscall(engine, syscall);

    // busy waiting returns
    while (engine->rb_ret_cons_idx == engine->rb_ret_prod_idx) {
        /* spinning */
        schedule();
    }

    // Consume ret
    spin_lock(&engine->lock);
    if (engine->rb_ret_cons_idx != engine->rb_ret_prod_idx) {
        struct hio_cmd_t *cmd = &engine->rb[engine->rb_ret_cons_idx];
        ret = cmd->ret_val;
        printk(KERN_INFO "HIO ENGINE: stub %d syscall %d returns %d\n", cmd->stub_id, cmd->syscall_nr, cmd->ret_val);
        engine->rb_ret_cons_idx = (engine->rb_ret_cons_idx + 1) % HIO_RB_SIZE;
    } else {
        printk(KERN_ERR "No pending syscall_ret found\n");
        ret = -1;
    }
    spin_unlock(&engine->lock);

    return ret;
} 

int hio_engine_init(struct hio_engine *hio_engine) {

    hio_engine->magic = 0x87654321;
    spin_lock_init(&hio_engine->lock);
    init_waitqueue_head(&hio_engine->syscall_wq);

    //printk(KERN_INFO "HIO: create hio_engine kthread\n");

    /*
    {
        // create polling kthread
        struct task_struct *handler_thread = kthread_run(hio_handler_worker, (void *)hio_engine, "hio_polling");
        if (IS_ERR(handler_thread)) {
            printk(KERN_ERR "Failed to start hio hanlder thread\n");
            return -1;
        }
        hio_engine->handler_thread = handler_thread;
    }
    */

    return 0;
}

int hio_engine_deinit(struct hio_engine *hio_engine) {
    //kthread_stop(hio_engine->handler_thread);
    return 0;
}


int 
add_stub(struct hio_engine *hio_engine, 
        int stub_id, struct hio_stub *stub) {
    if (hio_engine->stub_lookup_table[stub_id] != NULL) {
        printk(KERN_ERR "Failed to insert duplicated stub stub_id %d\n", stub_id);
        return -1;
    }
    hio_engine->stub_lookup_table[stub_id] = stub;
    return 0;
}


int
remove_stub(struct hio_engine *hio_engine, int stub_id) {
    int ret = 0;
    if (hio_engine->stub_lookup_table[stub_id] == NULL) {
        printk(KERN_WARNING "Trying to remove a non-existing stub, stub_id=%d\n", stub_id);
        return -1;
    }
    hio_engine->stub_lookup_table[stub_id] = NULL;
    return ret;
}


struct hio_stub * 
lookup_stub(struct hio_engine *hio_engine, int stub_id) {
    return hio_engine->stub_lookup_table[stub_id];
}
