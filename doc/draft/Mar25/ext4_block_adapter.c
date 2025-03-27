#include <kernel/device/block_device.h>
#include <kernel/fs/ext4_adaptor.h>
#include <kernel/fs/vfs/superblock.h>
#include <util/string.h>
#include <kernel/mm/kmalloc.h>

// 桥接结构体，存储在 ext4_blockdev 的 p_user 字段中
struct ext4_blockdev_linux {
    struct block_device *bdev;   // 指向 Linux VFS 块设备
    uint8_t *buffer;            // 临时读写缓冲区
};

// 适配函数: 打开块设备
static int ext4_linux_open(struct ext4_blockdev *ext4_bdev) {
    struct ext4_blockdev_linux *priv = (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
    if (!priv || !priv->bdev)
        return -EIO;
    
    return 0; // 假设设备已打开
}

// 适配函数: 关闭块设备
static int ext4_linux_close(struct ext4_blockdev *ext4_bdev) {
    return 0; // 不需要做额外关闭操作
}

// 适配函数: 读块
static int ext4_linux_bread(struct ext4_blockdev *ext4_bdev, void *buf, 
                          uint64_t blk_id, uint32_t blk_cnt) {
    struct ext4_blockdev_linux *priv = (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
    if (!priv || !priv->bdev)
        return -EIO;
    
    // 将 lwext4 偏移量转换为原始设备偏移量
    blk_id += ext4_bdev->part_offset;
    
    // 使用 Linux VFS 块操作读取数据
    return priv->bdev->bd_ops->read_blocks(priv->bdev, blk_id, buf, blk_cnt);
}

// 适配函数: 写块
static int ext4_linux_bwrite(struct ext4_blockdev *ext4_bdev, const void *buf, 
                           uint64_t blk_id, uint32_t blk_cnt) {
    struct ext4_blockdev_linux *priv = (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
    if (!priv || !priv->bdev)
        return -EIO;

    // 将 lwext4 偏移量转换为原始设备偏移量
    blk_id += ext4_bdev->part_offset;
    
    // 使用 Linux VFS 块操作写入数据
    return priv->bdev->bd_ops->write_block(priv->bdev, blk_id, buf, blk_cnt);
}

// 锁定和解锁函数
static int ext4_linux_lock(struct ext4_blockdev *ext4_bdev) {
    struct ext4_blockdev_linux *priv = (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
    if (priv && priv->bdev)
        spin_lock(&priv->bdev->bd_lock);
    return 0;
}

static int ext4_linux_unlock(struct ext4_blockdev *ext4_bdev) {
    struct ext4_blockdev_linux *priv = (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
    if (priv && priv->bdev)
        spin_unlock(&priv->bdev->bd_lock);
    return 0;
}

// 创建 lwext4 块设备结构，包装 Linux VFS 块设备
struct ext4_blockdev *ext4_blockdev_create_from_linux_bdev(struct block_device *bdev) {
    if (!bdev)
        return NULL;
    
    // 分配 ext4_blockdev 结构
    struct ext4_blockdev *ext4_bdev = kmalloc(sizeof(struct ext4_blockdev));
    if (!ext4_bdev)
        return NULL;
    
    // 分配接口结构
    struct ext4_blockdev_iface *bdif = kmalloc(sizeof(struct ext4_blockdev_iface));
    if (!bdif) {
        kfree(ext4_bdev);
        return NULL;
    }

    // 分配私有数据结构
    struct ext4_blockdev_linux *priv = kmalloc(sizeof(struct ext4_blockdev_linux));
    if (!priv) {
        kfree(bdif);
        kfree(ext4_bdev);
        return NULL;
    }

    // 分配临时缓冲区
    uint8_t *buffer = kmalloc(bdev->bd_block_size);
    if (!buffer) {
        kfree(priv);
        kfree(bdif);
        kfree(ext4_bdev);
        return NULL;
    }

    // 初始化私有数据
    priv->bdev = bdev;
    priv->buffer = buffer;
    
    // 设置接口
    bdif->open = ext4_linux_open;
    bdif->bread = ext4_linux_bread;
    bdif->bwrite = ext4_linux_bwrite;
    bdif->close = ext4_linux_close;
    bdif->lock = ext4_linux_lock;
    bdif->unlock = ext4_linux_unlock;
    bdif->ph_bsize = bdev->bd_block_size;
    bdif->ph_bcnt = bdev->bd_nr_blocks;
    bdif->ph_bbuf = buffer;
    bdif->ph_refctr = 1;
    bdif->bread_ctr = 0;
    bdif->bwrite_ctr = 0;
    bdif->p_user = priv;
    
    // 设置 ext4_blockdev
    ext4_bdev->bdif = bdif;
    ext4_bdev->part_offset = 0;
    ext4_bdev->part_size = bdif->ph_bcnt * bdif->ph_bsize;
    ext4_bdev->bc = NULL; // 将在 ext4_block_init 中设置
    ext4_bdev->lg_bsize = bdif->ph_bsize;
    ext4_bdev->lg_bcnt = bdif->ph_bcnt;
    ext4_bdev->cache_write_back = 0;
    ext4_bdev->fs = NULL; // 将会在 ext4_fs 初始化时设置
    ext4_bdev->journal = NULL;
    
    // 初始化块设备
    if (ext4_block_init(ext4_bdev) != 0) {
        kfree(buffer);
        kfree(priv);
        kfree(bdif);
        kfree(ext4_bdev);
        return NULL;
    }
    
    return ext4_bdev;
}

// 释放 lwext4 块设备结构
void ext4_blockdev_destroy(struct ext4_blockdev *ext4_bdev) {
    if (!ext4_bdev)
        return;
    
    // 关闭块设备
    ext4_block_fini(ext4_bdev);
    
    // 释放私有数据
    if (ext4_bdev->bdif && ext4_bdev->bdif->p_user) {
        struct ext4_blockdev_linux *priv = 
            (struct ext4_blockdev_linux *)ext4_bdev->bdif->p_user;
        
        if (priv->buffer)
            kfree(priv->buffer);
        
        kfree(priv);
    }
    
    // 释放接口结构
    if (ext4_bdev->bdif)
        kfree(ext4_bdev->bdif);
    
    // 释放块设备结构
    kfree(ext4_bdev);
}