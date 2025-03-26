// 建议的实现文件
#include <kernel/device/block_device.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/types.h>
#include <util/list.h>

// 全局块设备链表
struct list_head all_block_devices;
spinlock_t block_devices_lock;

// 创建块设备
struct block_device *alloc_block_device(void) {
    struct block_device *bdev = kmalloc(sizeof(struct block_device));
    if (!bdev)
        return NULL;
    
    memset(bdev, 0, sizeof(struct block_device));
    atomic_set(&bdev->bd_refcnt, 1);  // 初始引用计数为1
    spin_lock_init(&bdev->bd_lock);
    INIT_LIST_HEAD(&bdev->bd_list);
    
    return bdev;
}

// 释放块设备
void free_block_device(struct block_device *bdev) {
    if (!bdev)
        return;
    
    if (atomic_read(&bdev->bd_refcnt) > 0) {
        kprintf("Warning: freeing block device with references\n");
    }
    
    kfree(bdev);
}

// 获取块设备
struct block_device *get_block_device(dev_t dev, fmode_t mode) {
    struct block_device *bdev = NULL;
    
    spin_lock(&block_devices_lock);
    list_for_each_entry(bdev, &all_block_devices, bd_list) {
        if (bdev->bd_dev == dev) {
            atomic_inc(&bdev->bd_refcnt);
            spin_unlock(&block_devices_lock);
            
            // 打开设备
            if (blkdev_get(bdev, mode) != 0) {
                blkdev_put(bdev);
                return NULL;
            }
            
            return bdev;
        }
    }
    spin_unlock(&block_devices_lock);
    
    return NULL;
}

// 增加设备引用并打开
int blkdev_get(struct block_device *bdev, fmode_t mode) {
    int res = 0;
    
    if (bdev->bd_ops && bdev->bd_ops->open)
        res = bdev->bd_ops->open(bdev, mode);
    
    if (res == 0) {
        bdev->bd_mode |= mode;
        bdev->bd_openers++;
    }
    
    return res;
}

// 减少设备引用
void blkdev_put(struct block_device *bdev) {
    if (!bdev)
        return;
    
    if (atomic_dec_and_test(&bdev->bd_refcnt)) {
        if (bdev->bd_ops && bdev->bd_ops->release)
            bdev->bd_ops->release(bdev);
        
        // 从全局列表中移除
        spin_lock(&block_devices_lock);
        list_del(&bdev->bd_list);
        spin_unlock(&block_devices_lock);
        
        free_block_device(bdev);
    }
}

// 注册块设备
int register_blkdev(unsigned int major, const char *name, 
                   struct block_operations *ops) {
    // 实现注册逻辑
    // ...
    
    return 0;
}

// 初始化块设备层
void block_dev_init(void) {
    // 初始化块设备子系统
    INIT_LIST_HEAD(&all_block_devices);
    spinlock_init(&block_devices_lock);
    
    // 其他初始化
    // ...
}