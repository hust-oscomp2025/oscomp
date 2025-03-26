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
#include <kernel/fs/lwext4/ext4_dir_idx.h>
#include <kernel/fs/lwext4/ext4_dir.h>
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

#include <kernel/fs/vfs/vfs.h>
/*helper functions*/
inline void ext4_timestamp_to_timespec64(uint32_t timestamp, struct timespec64 *ts);
inline uint32_t timespec64_to_ext4_timestamp(struct timespec64 *ts);
void make_fsid_from_uuid(const uint8_t uuid[16], fsid_t *fsid);
/*global ext4 superblock lock*/
inline void ext4_lock(void);		// 注意，我们不使用ext4的mountpoint锁。
inline void ext4_unlock(void);





/**@brief   Mount point OS dependent lock*/
#define EXT4_MP_LOCK(_m)                                                       \
	do {                                                                   \
		if ((_m)->os_locks)                                            \
			(_m)->os_locks->lock();                                \
	} while (0)

/**@brief   Mount point OS dependent unlock*/
#define EXT4_MP_UNLOCK(_m)                                                     \
	do {                                                                   \
		if ((_m)->os_locks)                                            \
			(_m)->os_locks->unlock();                              \
	} while (0)

#endif