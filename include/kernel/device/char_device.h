#ifndef _CHAR_DEVICE_H
#define _CHAR_DEVICE_H

#include <kernel/types.h>
#include <kernel/util/atomic.h>

struct file;



struct char_device {
    dev_t cd_dev;                      /* Device number */
    const struct char_device_operations* ops;  /* Device operations */
    atomic_t cd_count;                 /* Reference count */
    void* private_data;                /* Driver private data */
};

struct char_device_operations {
    int32 (*open)(struct char_device*, struct file*);
    int32 (*release)(struct char_device*, struct file*);
    ssize_t (*read)(struct char_device*, struct file*, char*, size_t, loff_t*);
    ssize_t (*write)(struct char_device*, struct file*, const char*, size_t, loff_t*);
    loff_t (*llseek)(struct char_device*, struct file*, loff_t, int32);
    int64 (*ioctl)(struct char_device*, struct file*, uint32, uint64);
};

/* Character device management functions */
struct char_device* cdev_get(dev_t dev);
void cdev_put(struct char_device* cdev);

#endif /* _CHAR_DEVICE_H */