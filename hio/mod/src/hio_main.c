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

#include "hio_ioctl.h"                /* device file ioctls*/
#include "hio.h"

int                      hio_major_num = 0;
struct class           * hio_class     = NULL;

struct hio_engine * hio_engine = NULL;


static int 
device_open(struct inode * inode, 
	    struct file  * filp) 
{
    struct hio_engine *engine = container_of(inode->i_cdev, struct hio_engine, cdev);
    filp->private_data = engine;
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
    struct hio_engine * hio_engine = (struct hio_engine *)filp->private_data;

    if (hio_engine == NULL) {
	    printk(KERN_ERR "HIO: hio_engine is NULL\n");
	    return -1;
    }

    switch (ioctl) {
        /*
         * Register a stub process with hio_engine, create /dev/hio-stubN
         * Need to specify an ID N that is shared between stub and client process
         * The stub process then poll on the /dev/hio-stubN
         * Syscalls with the ID N will be routed to this stub process
         */
        case HIO_IOCTL_REGISTER: 
            {
                int id = arg;

                printk(KERN_INFO "HIO: ioctl register stub_id %d, create /dev/hio-stub%d\n", id, id);
                ret = stub_register(hio_engine, id);
                break;
            }

        case HIO_IOCTL_DEREGISTER:
            {
                unsigned long id = arg;
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

    printk(KERN_INFO "HIO: Load kernel module\n");

    hio_engine = kmalloc(sizeof(struct hio_engine), GFP_KERNEL);
    if (IS_ERR(hio_engine)) {
        printk(KERN_ERR "Error alloc hio_engine\n");
        return -1;
    }
    memset(hio_engine, 0, sizeof(struct hio_engine));

    if (hio_engine_init(hio_engine) < 0) {
        printk(KERN_ERR "Error init hio_engine\n");
        return -1;
    }

    if (alloc_chrdev_region(&dev_num, 0, MAX_STUBS + 1, "hio") < 0) {
    	hio_engine_deinit(hio_engine);
        printk(KERN_ERR "Error allocating hio char device region\n");
        return -1;
    }

    hio_major_num = MAJOR(dev_num);
    dev_num       = MKDEV(hio_major_num, MAX_STUBS + 1);

    //printk(KERN_INFO "<Major, Minor>: <%d, %d>\n", MAJOR(dev_num), MINOR(dev_num));

    if ((hio_class = class_create(THIS_MODULE, "hio")) == NULL) {
        printk(KERN_ERR "Error creating hio device class\n");
        unregister_chrdev_region(dev_num, 1);
    	hio_engine_deinit(hio_engine);
        return -1;
    }

    if (device_create(hio_class, NULL, dev_num, NULL, "hio") == NULL) {
        printk(KERN_ERR "Error creating hio device\n");
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
    	hio_engine_deinit(hio_engine);
        return -1;
    }

    cdev_init(&(hio_engine->cdev), &fops);

    if (cdev_add(&(hio_engine->cdev), dev_num, 1) == -1) {
        printk(KERN_ERR "Error adding hio cdev\n");
        device_destroy(hio_class, dev_num);
        class_destroy(hio_class);
        unregister_chrdev_region(dev_num, MAX_STUBS + 1);
    	hio_engine_deinit(hio_engine);
        return -1;
    }
    
    return 0;
}

void 
hio_exit(void) 
{
    dev_t dev_num = MKDEV(hio_major_num, MAX_STUBS + 1);
    hio_engine_deinit(hio_engine);
    unregister_chrdev_region(MKDEV(hio_major_num, 0), MAX_STUBS + 1);
    cdev_del(&(hio_engine->cdev));
    device_destroy(hio_class, dev_num);
    class_destroy(hio_class);
}



module_init(hio_init);
module_exit(hio_exit);

MODULE_LICENSE("GPL");
