#ifndef EXT4_ADAPTOR_H
#define EXT4_ADAPTOR_H
// 统一头文件入口：用于在兼容层使用 lwext4 所有接口
#include <kernel/fs/lwext4/ext4.h>
#include <kernel/fs/lwext4/ext4_balloc.h>
#include <kernel/fs/lwext4/ext4_bcache.h>
#include <kernel/fs/lwext4/ext4_bitmap.h>
#include <kernel/fs/lwext4/ext4_block_group.h>
#include <kernel/fs/lwext4/ext4_blockdev.h>
#include <kernel/fs/lwext4/ext4_config.h>
#include <kernel/fs/lwext4/ext4_crc32.h>
#include <kernel/fs/lwext4/ext4_debug.h>
#include <kernel/fs/lwext4/ext4_dir.h>
#include <kernel/fs/lwext4/ext4_dir_idx.h>
#include <kernel/fs/lwext4/ext4_errno.h>
#include <kernel/fs/lwext4/ext4_extent.h>
#include <kernel/fs/lwext4/ext4_fs.h>
#include <kernel/fs/lwext4/ext4_hash.h>
#include <kernel/fs/lwext4/ext4_ialloc.h>
#include <kernel/fs/lwext4/ext4_inode.h>
#include <kernel/fs/lwext4/ext4_journal.h>
#include <kernel/fs/lwext4/ext4_mbr.h>
#include <kernel/fs/lwext4/ext4_misc.h>
#include <kernel/fs/lwext4/ext4_mkfs.h>
#include <kernel/fs/lwext4/ext4_oflags.h>
#include <kernel/fs/lwext4/ext4_super.h>
#include <kernel/fs/lwext4/ext4_trans.h>
#include <kernel/fs/lwext4/ext4_types.h>
#include <kernel/fs/lwext4/ext4_xattr.h>

#include <kernel/vfs.h>
/*helper functions*/
void ext4_timestamp_to_timespec64(uint32_t timestamp, struct timespec* ts);

uint32_t timespec64_to_ext4_timestamp(struct timespec* ts);
void make_fsid_from_uuid(const uint8_t uuid[16], __kernel_fsid_t* fsid);

/*global ext4 superblock lock*/
extern spinlock_t ext4_spinlock;
static inline void ext4_lock(void) { spinlock_lock(&ext4_spinlock); }
static inline void ext4_unlock(void) { spinlock_unlock(&ext4_spinlock); }

int32 ext4_sync_inode(struct ext4_inode_ref* inode_ref);
int32 ext4_fs_sync(struct ext4_fs* fs);



/* Block device adapter */
struct ext4_blockdev* ext4_blockdev_create_adapter(struct block_device* kernel_bdev);
void ext4_blockdev_free_adapter(struct ext4_blockdev* e_blockdevice);


/* Forward declarations for file and dir operations */
extern const struct file_operations ext4_file_operations;
extern const struct file_operations ext4_dir_operations;

// int32 ext4_fs_flush_journal(struct ext4_fs *fs);

#endif