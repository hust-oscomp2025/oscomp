#include <kernel/fs/ext4_adaptor.h>
#include <kernel/vfs.h>

#include <kernel/fs/lwext4/ext4.h>
#include <kernel/fs/lwext4/ext4_inode.h>
#include <kernel/fs/lwext4/ext4_xattr.h>
#include <kernel/fs/lwext4/ext4_super.h>
#include <kernel/fs/lwext4/ext4_fs.h>
#include <kernel/fs/lwext4/ext4_dir.h>

#include <kernel/util/spinlock.h>

spinlock_t ext4_spinlock = SPINLOCK_INIT;

static inline void ext4_lock_noop(void) {
    // no-op placeholder: intentionally left blank
}

struct ext4_lock ext4_mount_lock = {
	.lock = ext4_lock_noop,
	.unlock = ext4_lock_noop
};




/**
 * ext4_timestamp_to_timespec64 - Convert ext4 timestamp to timespec
 * @timestamp: ext4 timestamp (uint32)
 * @ts: pointer to timespec structure to fill
 *
 * Converts an ext4 timestamp value to the VFS timespec format
 */
void ext4_timestamp_to_timespec64(uint32_t timestamp, struct timespec *ts)
{
    /* Unix epoch time is stored in seconds */
    ts->tv_sec = timestamp;
    ts->tv_nsec = 0;
}


/**
 * timespec64_to_ext4_timestamp
 * 
 * 
 */
uint32_t timespec64_to_ext4_timestamp(struct timespec *ts)
{
	return ts->tv_sec;
}

void make_fsid_from_uuid(const uint8_t uuid[16], fsid_t *fsid) {
    const uint32_t *u = (const uint32_t *)uuid;
    fsid->__val[0] = (int32)(u[0] ^ u[1]);
    fsid->__val[1] = (int32)(u[2] ^ u[3]);
}