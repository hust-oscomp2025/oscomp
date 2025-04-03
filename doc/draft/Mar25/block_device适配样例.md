// ext4 适配示例
struct ext4_blockdev *create_ext4_blockdev(struct block_device *bdev) {
    struct ext4_blockdev *ext4_bdev = kmalloc(sizeof(struct ext4_blockdev));
    if (!ext4_bdev)
        return NULL;
        
    // 设置 ext4_blockdev 属性
    ext4_bdev->part_offset = 0;
    ext4_bdev->part_size = bdev->bd_nr_blocks * bdev->bd_block_size;
    ext4_bdev->bdif = kmalloc(sizeof(struct ext4_blockdev_iface));
    if (!ext4_bdev->bdif) {
        kfree(ext4_bdev);
        return NULL;
    }
    
    // 设置接口和私有数据
    ext4_bdev->bdif->ph_bcnt = bdev->bd_nr_blocks;
    ext4_bdev->bdif->ph_bsize = bdev->bd_block_size;
    ext4_bdev->bdif->p_user = bdev;  // 存储原始 bdev 以便操作
    
    // 设置操作函数
    // ...
    
    return ext4_bdev;
}