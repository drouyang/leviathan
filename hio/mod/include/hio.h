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

struct hio_engine {
    int test;
};


struct hio_stub {
    int app_id;
    struct hio_engine *hio_engine;

    dev_t       dev; 
    struct cdev cdev;
};

#endif
