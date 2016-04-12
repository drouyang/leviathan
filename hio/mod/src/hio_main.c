/* 
 * HIO Main Interface
 * (c) 2016, Jiannan Ouyang <ouyang@cs.pitt.edu>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>             /* device file */
#include <linux/types.h>          /* dev_t */
#include <linux/kdev_t.h>         /* MAJOR MINOR MKDEV */
#include <linux/device.h>         /* udev */
#include <linux/cdev.h>           /* cdev_init cdev_add */
#include <linux/moduleparam.h>    /* module_param */
#include <linux/stat.h>           /* perms */
#include <asm/uaccess.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/pagemap.h>

#include <xpmem.h>
#include "hio_ioctl.h"                /* device file ioctls*/
#include "hio.h"

int                      hio_major_num  = 0;
struct class            *hio_class      = NULL;
struct cdev cdev;
struct hio_engine       *hio_engine     = NULL;
struct page *shared_page;
int *shared_page_ptr;

extern int64_t xpmem_get_domid(void);
extern int64_t
xpmem_make(u64 vaddr, size_t size, int permit_type, void *permit_value, int flags,
        xpmem_segid_t request, xpmem_segid_t *segid_p, int *fd_p);

static int 
device_open(struct inode * inode, 
	    struct file  * filp) 
{
    return 0;
}


static int 
device_release(struct inode * inode,
	       struct file  * file)
{
    return 0;
}

static ssize_t 
device_read(struct file * file, 
	    char __user * buffer,
	    size_t        length, 
	    loff_t      * offset)
{
    printk(KERN_INFO "Read\n");
    return -EINVAL;
}


static ssize_t 
device_write(struct file       * file,
	     const char __user * buffer,
	     size_t              length, 
	     loff_t            * offset) 
{
    printk(KERN_INFO "Write\n");
    return -EINVAL;
}


static long 
device_ioctl(struct file  * filp,
	     unsigned int   ioctl,
	     unsigned long  arg) 
{
    int ret = 0;
    //void __user * argp = (void __user *)arg;
    //struct hio_engine * hio_engine = (struct hio_engine *)filp->private_data;

    switch (ioctl) {
        /*
         * User process trap into kernel as a kernel thread
         */
        case HIO_IOCTL_ENGINE_START:
            {
                unsigned long engine_kva = 0;
                /* grap the memory passed from user space */
                {
                    //void *engine_uva = (void *) arg;
                    //phys_addr_t engine_pa = virt_to_phys(engine_uva);
                    //void * engine_kva = phys_to_virt(engine_pa);
                    //printk(KERN_INFO "uva %p, pa %p, kva %p\n", engine_uva, (void *) engine_pa, engine_kva);

                    unsigned long uaddr = arg;
                    int res;

                    down_read(&current->mm->mmap_sem);
                    res = get_user_pages(current, current->mm,
                            uaddr,
                            1, /* Only want one page */
                            1, /* Do want to write into it */
                            1, /* do force */
                            &shared_page,
                            NULL);
                    if (res == 1) {
                        shared_page_ptr = kmap(shared_page);
                        pr_info("Got page: kva %p, kpa %p, uva %lx, content 0x%x\n", 
                                shared_page_ptr, (void *)virt_to_phys(shared_page_ptr), uaddr, *shared_page_ptr);
                        *shared_page_ptr = 0x11111111;
                    } else {
                        pr_err("Couldn't get page :(\n");
                        up_read(&current->mm->mmap_sem);
                        return -1;
                    }
                    up_read(&current->mm->mmap_sem);
                }
                break;

                hio_engine = (struct hio_engine *) engine_kva;
                if (hio_engine_init(hio_engine) < 0) {
                    printk(KERN_ERR "Error init hio_engine\n");
                    ret = -1;
                }

                hio_engine_event_loop(hio_engine);

                break;
            }
        /*
         * Register a stub process with hio_engine, create /dev/hio-stubN
         * Need to specify an ID N that is shared between stub and client process
         * The stub process then poll on the /dev/hio-stubN
         * Syscalls with the ID N will be routed to this stub process
         */
        case HIO_IOCTL_REGISTER: 
            {
                int id = arg;

                if (hio_engine == NULL) {
                    printk(KERN_ERR "HIO: hio_engine is NULL\n");
                    return -1;
                }

                printk(KERN_INFO "HIO: ioctl register stub_id %d, create /dev/hio-stub%d\n", id, id);
                ret = stub_register(hio_engine, id);
                break;
            }

        case HIO_IOCTL_DEREGISTER:
            {
                unsigned long id = arg;

                if (hio_engine == NULL) {
                    printk(KERN_ERR "HIO: hio_engine is NULL\n");
                    return -1;
                }

                ret = stub_deregister(hio_engine, id);
                break;
            }

        default:
            printk(KERN_ERR "Invalid HIO IOCTL: %d\n", ioctl);
            ret = -EINVAL;
    }

    return ret;
}


static struct file_operations fops = {
    .owner            = THIS_MODULE,
    .read             = device_read,
    .write            = device_write,
    .unlocked_ioctl   = device_ioctl,
    .compat_ioctl     = device_ioctl,
    .open             = device_open,
    .release          = device_release
};


int 
hio_init(void) 
{
    dev_t dev_num   = MKDEV(0, 0); // <major , minor> 

    printk(KERN_INFO "HIO: load kernel module...\n");
    printk(KERN_INFO "    minimal shared memory size: %lu\n", sizeof(struct hio_engine));

#if 0
    hio_engine = kmalloc(sizeof(struct hio_engine), GFP_KERNEL);
    if (IS_ERR(hio_engine)) {
        printk(KERN_ERR "Error alloc hio_engine\n");
        return -1;
    }
    memset(hio_engine, 0, sizeof(struct hio_engine));

    /* export xpmem region */
    {
#define HIO_XPMEM_MAGIC 16
        xpmem_segid_t segid;

        //xpmem_make(u64 vaddr, size_t size, int permit_type, void *permit_value, int flags,
        //       xpmem_segid_t request, xpmem_segid_t *segid_p, int *fd_p)
        //
        printk(KERN_INFO "Exporting xpmem memory\n");
        xpmem_get_domid();
        xpmem_make((u64)hio_engine, sizeof(struct hio_engine), XPMEM_GLOBAL_MODE, 
                (void *)0, XPMEM_REQUEST_MODE, HIO_XPMEM_MAGIC, &segid, NULL);
        if (segid != HIO_XPMEM_MAGIC) {
            printk(KERN_ERR "xpmem returns segid %lld does not match %d\n", 
                    segid, HIO_XPMEM_MAGIC);
            return -1;
        }
    }

    if (hio_engine_init(hio_engine) < 0) {
        printk(KERN_ERR "Error init hio_engine\n");
        return -1;
    }
#endif

    if (alloc_chrdev_region(&dev_num, 0, MAX_STUBS + 1, "hio") < 0) {
        printk(KERN_ERR "Error allocating hio char device region\n");
        return -1;
    }

    hio_major_num = MAJOR(dev_num);
    dev_num       = MKDEV(hio_major_num, MAX_STUBS + 1);

    //printk(KERN_INFO "<Major, Minor>: <%d, %d>\n", MAJOR(dev_num), MINOR(dev_num));

    if ((hio_class = class_create(THIS_MODULE, "hio")) == NULL) {
        printk(KERN_ERR "Error creating hio device class\n");
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    if (device_create(hio_class, NULL, dev_num, NULL, "hio") == NULL) {
        printk(KERN_ERR "Error creating hio device\n");
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
        return -1;
    }

    cdev_init(&cdev, &fops);

    if (cdev_add(&cdev, dev_num, 1) == -1) {
        printk(KERN_ERR "Error adding hio cdev\n");
        device_destroy(hio_class, dev_num);
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
        return -1;
    }
    
    return 0;
}

void 
hio_exit(void) 
{
    struct hio_stub *stub = NULL;
    int i;
    dev_t dev_num;

    printk(KERN_INFO "HIO: remove kernel module...\n");

    if (hio_engine != NULL) {
        for(i = 0; i < MAX_STUBS; i++) {
            stub = hio_engine->stub_lookup_table[i];
            if (stub != NULL) {
                printk(KERN_INFO "HIO: destory stub %d (/dev/hio-stub%d)\n", stub->stub_id, stub->stub_id);
                stub_deregister(hio_engine, stub->stub_id);
            }
        }
    }
/*
   kunmap(shared_page);

   if (!PageReserved(shared_page))
   SetPageDirty(shared_page);
   page_cache_release(shared_page);
*/

    dev_num = MKDEV(hio_major_num, MAX_STUBS + 1);
    if (hio_engine != NULL) hio_engine_deinit(hio_engine);
    unregister_chrdev_region(MKDEV(hio_major_num, 0), MAX_STUBS + 1);
    cdev_del(&cdev);
    device_destroy(hio_class, dev_num);
    class_destroy(hio_class);
}



module_init(hio_init);
module_exit(hio_exit);

MODULE_LICENSE("GPL");
