#ifndef EXT4_BLOCK_ADAPTER_H
#define EXT4_BLOCK_ADAPTER_H

#include <kernel/device/block_device.h>
#include <kernel/fs/ext4_adaptor.h>

// 创建 lwext4 块设备结构，包装 Linux VFS 块设备
struct ext4_blockdev *ext4_blockdev_create_from_linux_bdev(struct block_device *bdev);

// 释放 lwext4 块设备结构
void ext4_blockdev_destroy(struct ext4_blockdev *ext4_bdev);

// 处理 ext4_mount_point_stats 问题的辅助函数
static inline int ext4_get_fs_stats(struct ext4_fs *fs, struct ext4_mount_stats *stats) {
    if (!fs || !stats)
        return -EINVAL;
    
    // 获取 fs 中的基本信息填充到 stats 中
    stats->block_size = ext4_sb_get_block_size(fs->sb);
    stats->blocks_count = ext4_sb_get_blocks_cnt(fs->sb);
    stats->free_blocks_count = ext4_sb_get_free_blocks_cnt(fs->sb);
    stats->inodes_count = ext4_sb_get_inodes_cnt(fs->sb);
    stats->free_inodes_count = ext4_sb_get_free_inodes_cnt(fs->sb);
    
    return 0;
}

// 修改 ext4_statfs 函数中的问题
static inline int ext4_get_stats_from_sb(struct superblock *sb, struct ext4_mount_stats *stats) {
    struct ext4_fs *fs = sb->s_fs_info;
    if (!fs || !stats)
        return -EINVAL;
    
    return ext4_get_fs_stats(fs, stats);
}

#endif // EXT4_BLOCK_ADAPTER_H