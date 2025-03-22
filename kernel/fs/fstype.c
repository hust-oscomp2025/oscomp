#include <kernel/fs/vfs.h>
#include <kernel/types.h>

/**
 * fsType_createMount - Mount a filesystem
 * @type: Filesystem type
 * @flags: Mount flags
 * @dev_name: Device name (can be NULL for virtual filesystems)
 * @data: Filesystem-specific mount options
 *
 * Mounts a filesystem of the specified type.
 *
 * Returns the superblock on success, ERR_PTR on failure
 */
struct superblock* fsType_createMount(struct fsType* type, int flags, const char* dev_name, void* data) {
	struct superblock* sb;
	int error;
	dev_t dev_id = 0; /* Default to 0 for virtual filesystems */

	if (unlikely(!type || !type->fs_mount_sb))
		return ERR_PTR(-ENODEV);

	/* Get device ID if we have a device name */
	if (dev_name && *dev_name) {
		error = lookup_dev_id(dev_name, &dev_id);
		if (error)
			return ERR_PTR(error);
	}

	/* Get or allocate superblock */
	sb = fsType_acquireSuperblock(type, dev_id, data);
	if (!sb)
		return ERR_PTR(-ENOMEM);

	/* Set flags */
	sb->s_flags = flags;

	/* If this is a new superblock (no root yet), initialize it */
	if (sb->s_global_root_dentry == NULL) {
		/* Call fs_fill_sb if available */
		if (type->fs_fill_sb) {
			error = type->fs_fill_sb(sb, data, flags);
			if (error) {
				drop_super(sb);
				return ERR_PTR(error);
			}
		}
		/* Or call mount if fs_fill_sb isn't available */
		else if (type->fs_mount_sb) {
			/* This is a fallback - ideally all filesystems would
			 * implement fs_fill_sb instead */
			struct superblock* new_sb;
			new_sb = type->fs_mount_sb(type, flags, dev_name, data);
			if (IS_ERR(new_sb)) {
				drop_super(sb);
				return new_sb;
			}
			/* This would need to handle merging the superblocks
			 * but it's a non-standard path */
			drop_super(sb);
			sb = new_sb;
		}
	}

	/* Increment active count */
	grab_super(sb);

	return sb;
}


/**
 * fsType_acquireSuperblock - Get or create a superblock
 * @type: Filesystem type
 * @dev_id: Device identifier (0 for virtual filesystems)
 * @fs_data: Filesystem-specific mount data
 *
 * Returns an existing superblock for the device or creates a new one.
 * Increments the reference count on the returned superblock.
 *
 * Returns: pointer to superblock or NULL on failure
 */
struct superblock* fsType_acquireSuperblock(struct fsType* type, dev_t dev_id, void* fs_data) {
	struct superblock* sb = NULL;

	if (!type)
		return NULL;

	/* Lock to protect the filesystem type's superblock list */
	spin_lock(&type->fs_list_s_lock);

	/* Check if a superblock already exists for this device */
	if (dev_id != 0) {
		list_for_each_entry(sb, &type->fs_list_sb, s_node_fsType) {
			if (sb->s_device_id == dev_id) {
				/* Found matching superblock - increment reference */
				sb->s_refcount++;
				spin_unlock(&type->fs_list_s_lock);
				return sb;
			}
		}
	}

	/* No existing superblock found, allocate a new one */
	spin_unlock(&type->fs_list_s_lock);
	sb = __alloc_super(type);
	if (!sb)
		return NULL;

	/* Set device ID */
	sb->s_device_id = dev_id;

	/* Store filesystem-specific data if provided */
	if (fs_data) {
		/* Note: Filesystem is responsible for managing this data */
		sb->s_fs_specific = fs_data;
	}

	/* Add to the filesystem's list of superblocks */
	spin_lock(&type->fs_list_s_lock);
	list_add(&sb->s_node_fsType, &type->fs_list_sb);
	spin_unlock(&type->fs_list_s_lock);

	return sb;
}