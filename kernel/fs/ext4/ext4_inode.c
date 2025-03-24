#include <kernel/fs/ext4_adaptor.h>
#include <kernel/fs/vfs/vfs.h>
#include <kernel/fs/vfs/inode.h>
#include <kernel/mm/kmalloc.h>

#include <vendor/lwext4/include/ext4.h>
#include <vendor/lwext4/include/ext4_inode.h>
#include <vendor/lwext4/include/ext4_xattr.h>
#include <vendor/lwext4/include/ext4_super.h>
#include <vendor/lwext4/include/ext4_fs.h>
#include <vendor/lwext4/include/ext4_dir.h>





/* Forward declarations for file and dir operations */
extern const struct file_operations ext4_file_operations;
extern const struct file_operations ext4_dir_operations;
static struct inode *ext4_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
static int ext4_create(struct inode *dir, struct dentry *dentry, mode_t mode);
static int ext4_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry);
static int ext4_unlink(struct inode *dir, struct dentry *dentry);
static int ext4_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
static int ext4_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode);
static int ext4_rmdir(struct inode *dir, struct dentry *dentry);
static int ext4_rename(struct inode *old_dir, struct dentry *old_dentry, 
                        struct inode *new_dir, struct dentry *new_dentry);

static int ext4_readlink(struct dentry *dentry, char *buffer, int buflen);
static int ext4_permission(struct inode *inode, int mask);
static int ext4_setattr(struct dentry *dentry, struct iattr *attr);
static int ext4_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat);
static int ext4_setxattr(struct dentry *dentry, const char *name, const void *value, 
                          size_t size, int flags);
static ssize_t ext4_getxattr(struct dentry *dentry, const char *name, void *buffer, size_t size);
static ssize_t ext4_listxattr(struct dentry *dentry, char *buffer, size_t size);
static int ext4_removexattr(struct dentry *dentry, const char *name);

/**
 * ext4_inode_init - Initialize a VFS inode from an ext4 inode
 * @sb: The superblock
 * @inode: The VFS inode to initialize
 * @ino: Inode number
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ext4_inode_init(struct superblock *sb, struct inode *inode, uint32_t ino) {
    struct ext4_inode_ref inode_ref;
    struct ext4_fs *fs = sb->s_fs_info;
    int ret;
    
    /* Get the ext4 inode reference */
    ret = ext4_fs_get_inode_ref(fs, ino, &inode_ref);
    if (ret != 0)
        return ret;
    
    /* Initialize the VFS inode from ext4 inode */
    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_mode = ext4_inode_get_mode(fs->sb, inode_ref.inode);
    inode->i_uid = ext4_inode_get_uid(inode_ref.inode);
    inode->i_gid = ext4_inode_get_gid(inode_ref.inode);
    inode->i_size = ext4_inode_get_size(fs->sb, inode_ref.inode);
    inode->i_atime = ext4_inode_get_access_time(inode_ref.inode);
    inode->i_mtime = ext4_inode_get_modif_time(inode_ref.inode);
    inode->i_ctime = ext4_inode_get_change_inode_time(inode_ref.inode);
    inode->i_blocks = ext4_inode_get_blocks_count(fs->sb, inode_ref.inode);
    inode->i_nlink = ext4_inode_get_links_cnt(inode_ref.inode);
    
    /* Set appropriate operations based on file type */
    if (S_ISREG(inode->i_mode)) {
        inode->i_op = &ext4_file_inode_operations;
        inode->i_fop = &ext4_file_operations;
    } else if (S_ISDIR(inode->i_mode)) {
        inode->i_op = &ext4_dir_inode_operations;
        inode->i_fop = &ext4_dir_operations;
    } else if (S_ISLNK(inode->i_mode)) {
        inode->i_op = &ext4_symlink_inode_operations;
        inode->i_fop = NULL;
    } else if (S_ISBLK(inode->i_mode) || S_ISCHR(inode->i_mode)) {
        /* Handle device files */
        inode->i_rdev = ext4_inode_get_dev(inode_ref.inode);
        inode->i_op = &ext4_file_inode_operations;
        inode->i_fop = NULL; /* Special device operations would go here */
    } else {
        /* FIFO, socket, etc. */
        inode->i_op = &ext4_file_inode_operations;
        inode->i_fop = NULL;
    }
    
    /* Release the ext4 inode reference */
    ext4_fs_put_inode_ref(&inode_ref);
    
    return 0;
}

/**
 * ext4_read_inode - Read the inode data from disk
 * @inode: The VFS inode to fill
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ext4_read_inode(struct inode *inode) {
    if (!inode || !inode->i_sb)
        return -EINVAL;
    
    return ext4_inode_init(inode->i_sb, inode, inode->i_ino);
}
static int get_ext4_inode_ref(struct inode *inode, struct ext4_inode_ref *ref) {
    struct ext4_fs *fs = inode->i_sb->s_fs_info;
    return ext4_fs_get_inode_ref(fs, inode->i_ino, ref);
}

static void put_ext4_inode_ref(struct ext4_inode_ref *ref) {
    ext4_fs_put_inode_ref(ref);
}

/* Implementation of inode operations */

/**
 * ext4_lookup - Look up a directory entry by name
 * @dir: Directory inode to search in
 * @dentry: Target directory entry to look up
 * @flags: Lookup flags
 * 
 * Returns: The inode corresponding to @dentry on success or NULL on failure
 */
static struct inode *ext4_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    struct ext4_inode_ref dir_ref;
    struct ext4_dir_search_result result;
    struct inode *inode = NULL;
    int ret;
    
    if (!dir || !dentry)
        return ERR_PTR(-EINVAL);
    
    /* Get ext4 inode reference for the directory */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ERR_PTR(ret);
    
    /* Call ext4 directory find entry function */
    ret = ext4_dir_find_entry(&result, &dir_ref, dentry->d_name->name, 
                               dentry->d_name->len);
    
    /* Release directory reference */
    put_ext4_inode_ref(&dir_ref);
    
    if (ret != 0) {
        /* Entry not found or error occurred */
        if (ret == -ENOENT)
            return NULL; /* Not an error, just not found */
        return ERR_PTR(ret);
    }
    
    /* Found the entry, now get the inode */
    inode = inode_get(dir->i_sb, result.dentry->inode);
    
    /* Clean up the search result */
    ext4_dir_destroy_result(&dir_ref, &result);
    
    return inode;
}

/**
 * ext4_create - Create a new regular file
 * @dir: Parent directory inode
 * @dentry: Directory entry for the new file
 * @mode: Mode bits for the new file
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_create(struct inode *dir, struct dentry *dentry, mode_t mode) {
    struct ext4_inode_ref dir_ref, inode_ref;
    struct inode *inode;
    int ret;
    
    /* Get directory inode reference */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    /* Allocate a new inode */
    ret = ext4_fs_alloc_inode(dir_ref.fs, &inode_ref, EXT4_DE_REG_FILE);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Set mode (permissions) for the new inode */
    ext4_inode_set_mode(dir_ref.fs->sb, inode_ref.inode, mode);
    
    /* Add entry to directory */
    ret = ext4_dir_add_entry(&dir_ref, dentry->d_name->name, dentry->d_name->len, &inode_ref);
    if (ret != 0) {
        /* Free the inode if we couldn't add the directory entry */
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Create VFS inode and add to dentry */
    inode = inode_create(dir->i_sb, inode_ref.index);
    if (!inode) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return -ENOMEM;
    }
    
    /* Initialize the VFS inode */
    inode->i_mode = mode;
    inode->i_op = &ext4_inode_operations;
    inode->i_fop = &ext4_file_operations;
    
    /* Link the dentry to the inode */
    dentry_instantiate(dentry, inode);
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_link - Create a hard link
 * @old_dentry: The existing dentry
 * @dir: The target directory
 * @new_dentry: The new dentry to create
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry) {
    struct ext4_inode_ref dir_ref, inode_ref;
    int ret;
    
    /* Get directory and source inode references */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    ret = get_ext4_inode_ref(old_dentry->d_inode, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Add entry to target directory */
    ret = ext4_dir_add_entry(&dir_ref, new_dentry->d_name->name, 
                             new_dentry->d_name->len, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Increment link count */
    ext4_fs_inode_links_count_inc(&inode_ref);
    
    /* Link the dentry to the inode */
    dentry_instantiate(new_dentry, inode_get(old_dentry->d_inode));
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_unlink - Remove a file entry from a directory
 * @dir: Parent directory inode
 * @dentry: The entry to remove
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_unlink(struct inode *dir, struct dentry *dentry) {
    struct ext4_inode_ref dir_ref, inode_ref;
    int ret;
    
    /* Get directory and target inode references */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    ret = get_ext4_inode_ref(dentry->d_inode, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Remove directory entry */
    ret = ext4_dir_remove_entry(&dir_ref, dentry->d_name->name, dentry->d_name->len);
    if (ret != 0) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Decrement link count */
    ext4_fs_inode_links_count_dec(&inode_ref);
    
    /* If this was the last link, mark the inode for deletion */
    if (ext4_inode_get_links_cnt(inode_ref.inode) == 0) {
        /* Set deletion time */
        ext4_inode_set_del_time(inode_ref.inode, time(NULL));
        /* Remove the inode from the filesystem */
        ext4_fs_free_inode(&inode_ref);
    }
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_symlink - Create a symbolic link
 * @dir: Parent directory inode
 * @dentry: The symlink entry
 * @symname: The target path
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_symlink(struct inode *dir, struct dentry *dentry, const char *symname) {
    struct ext4_inode_ref dir_ref, inode_ref;
    struct inode *inode;
    int ret;
    size_t symname_len = strlen(symname);
    
    /* Get directory inode reference */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    /* Allocate a new inode for the symlink */
    ret = ext4_fs_alloc_inode(dir_ref.fs, &inode_ref, EXT4_DE_SYMLINK);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Set mode for the symlink */
    ext4_inode_set_mode(dir_ref.fs->sb, inode_ref.inode, S_IFLNK | 0777);
    
    /* Add entry to directory */
    ret = ext4_dir_add_entry(&dir_ref, dentry->d_name->name, dentry->d_name->len, &inode_ref);
    if (ret != 0) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Store the symlink target */
    if (symname_len <= 60) {
        /* Store directly in the inode */
        memcpy(inode_ref.inode->blocks, symname, symname_len);
        ext4_inode_set_size(inode_ref.inode, symname_len);
    } else {
        /* Store in a separate block */
        ext4_fsblk_t fblock;
        ext4_lblk_t iblock = 0;
        
        /* Append a data block to the inode */
        ret = ext4_fs_append_inode_dblk(&inode_ref, &fblock, &iblock);
        if (ret != 0) {
            ext4_fs_free_inode(&inode_ref);
            put_ext4_inode_ref(&dir_ref);
            return ret;
        }
        
        /* Write the symlink target to the data block */
        struct ext4_block block;
        ret = ext4_block_get(dir_ref.fs->bdev, &block, fblock);
        if (ret != 0) {
            ext4_fs_free_inode(&inode_ref);
            put_ext4_inode_ref(&dir_ref);
            return ret;
        }
        
        memcpy(block.data, symname, symname_len);
        ext4_block_set(dir_ref.fs->bdev, &block);
        ext4_inode_set_size(inode_ref.inode, symname_len);
    }
    
    /* Create VFS inode and add to dentry */
    inode = inode_create(dir->i_sb, inode_ref.index);
    if (!inode) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return -ENOMEM;
    }
    
    /* Initialize the VFS inode */
    inode->i_mode = S_IFLNK | 0777;
    inode->i_op = &ext4_symlink_inode_operations;
    
    /* Link the dentry to the inode */
    dentry_instantiate(dentry, inode);
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_mkdir - Create a new directory
 * @dir: Parent directory inode
 * @dentry: Directory entry for the new directory
 * @mode: Mode bits for the new directory
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_mkdir(struct inode *dir, struct dentry *dentry, mode_t mode) {
    struct ext4_inode_ref dir_ref, inode_ref;
    struct inode *inode;
    int ret;
    
    /* Ensure mode includes directory flag */
    mode |= S_IFDIR;
    
    /* Get directory inode reference */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    /* Allocate a new inode for the directory */
    ret = ext4_fs_alloc_inode(dir_ref.fs, &inode_ref, EXT4_DE_DIR);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Set mode for the new directory */
    ext4_inode_set_mode(dir_ref.fs->sb, inode_ref.inode, mode);
    
    /* Initialize directory structure (create "." and "..") */
#if CONFIG_DIR_INDEX_ENABLE
    ret = ext4_dir_dx_init(&inode_ref, &dir_ref);
#else
    /* Create dot entries manually if directory indexing is disabled */
    struct ext4_block block;
    ext4_fsblk_t fblock;
    ext4_lblk_t iblock = 0;
    
    /* Append a data block to the inode */
    ret = ext4_fs_append_inode_dblk(&inode_ref, &fblock, &iblock);
    if (ret != 0) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Get the block to write directory entries */
    ret = ext4_block_get(dir_ref.fs->bdev, &block, fblock);
    if (ret != 0) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Create "." entry */
    struct ext4_dir_en *entry = (struct ext4_dir_en *)block.data;
    ext4_dir_write_entry(dir_ref.fs->sb, entry, 12, &inode_ref, ".", 1);
    
    /* Create ".." entry */
    entry = (struct ext4_dir_en *)((char *)block.data + 12);
    ext4_dir_write_entry(dir_ref.fs->sb, entry, block.size - 12, &dir_ref, "..", 2);
    
    /* Write the block back */
    ext4_block_set(dir_ref.fs->bdev, &block);
#endif
    
    if (ret != 0) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Add entry to parent directory */
    ret = ext4_dir_add_entry(&dir_ref, dentry->d_name->name, dentry->d_name->len, &inode_ref);
    if (ret != 0) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Create VFS inode and add to dentry */
    inode = inode_create(dir->i_sb, inode_ref.index);
    if (!inode) {
        ext4_fs_free_inode(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return -ENOMEM;
    }
    
    /* Initialize the VFS inode */
    inode->i_mode = mode;
    inode->i_op = &ext4_dir_inode_operations;
    inode->i_fop = &ext4_dir_operations;
    
    /* Link the dentry to the inode */
    dentry_instantiate(dentry, inode);
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_rmdir - Remove a directory
 * @dir: Parent directory inode
 * @dentry: The directory to remove
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_rmdir(struct inode *dir, struct dentry *dentry) {
    struct ext4_inode_ref dir_ref, inode_ref;
    struct ext4_dir_iter it;
    bool is_empty = true;
    int ret;
    
    /* Get directory and target directory references */
    ret = get_ext4_inode_ref(dir, &dir_ref);
    if (ret != 0)
        return ret;
    
    ret = get_ext4_inode_ref(dentry->d_inode, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Check if directory is empty */
    ret = ext4_dir_iterator_init(&it, &inode_ref, 0);
    if (ret != 0) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Skip "." and ".." entries */
    if (!it.curr) {
        is_empty = true;
    } else {
        /* Skip "." */
        ret = ext4_dir_iterator_next(&it);
        if (ret != 0) {
            ext4_dir_iterator_fini(&it);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&dir_ref);
            return ret;
        }
        
        /* Skip ".." */
        ret = ext4_dir_iterator_next(&it);
        if (ret != 0) {
            ext4_dir_iterator_fini(&it);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&dir_ref);
            return ret;
        }
        
        /* If there are more entries, the directory is not empty */
        is_empty = (it.curr == NULL);
    }
    
    ext4_dir_iterator_fini(&it);
    
    if (!is_empty) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return -ENOTEMPTY;
    }
    
    /* Remove directory entry from parent */
    ret = ext4_dir_remove_entry(&dir_ref, dentry->d_name->name, dentry->d_name->len);
    if (ret != 0) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&dir_ref);
        return ret;
    }
    
    /* Free the inode */
    ext4_fs_free_inode(&inode_ref);
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&dir_ref);
    
    return 0;
}

/**
 * ext4_rename - Rename a file or directory
 * @old_dir: Parent directory of source
 * @old_dentry: Source dentry
 * @new_dir: Parent directory of destination
 * @new_dentry: Destination dentry
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ext4_rename(struct inode *old_dir, struct dentry *old_dentry, 
                       struct inode *new_dir, struct dentry *new_dentry) {
    struct ext4_inode_ref old_dir_ref, new_dir_ref, inode_ref;
    struct ext4_dir_search_result result;
    int ret;
    
    /* Get inode references */
    ret = get_ext4_inode_ref(old_dir, &old_dir_ref);
    if (ret != 0)
        return ret;
    
    ret = get_ext4_inode_ref(new_dir, &new_dir_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&old_dir_ref);
        return ret;
    }
    
    ret = get_ext4_inode_ref(old_dentry->d_inode, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&new_dir_ref);
        put_ext4_inode_ref(&old_dir_ref);
        return ret;
    }
    
    /* Check if target already exists */
    ret = ext4_dir_find_entry(&result, &new_dir_ref, new_dentry->d_name->name, 
                               new_dentry->d_name->len);
    if (ret == 0) {
        /* Target exists, handle differently based on type */
        struct ext4_inode_ref target_ref;
        ret = get_ext4_inode_ref(new_dentry->d_inode, &target_ref);
        if (ret != 0) {
            ext4_dir_destroy_result(&new_dir_ref, &result);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&new_dir_ref);
            put_ext4_inode_ref(&old_dir_ref);
            return ret;
        }
        
        /* Check if source is a directory */
        bool source_is_dir = S_ISDIR(old_dentry->d_inode->i_mode);
        bool target_is_dir = S_ISDIR(new_dentry->d_inode->i_mode);
        
        if (source_is_dir && !target_is_dir) {
            /* Can't overwrite non-directory with directory */
            put_ext4_inode_ref(&target_ref);
            ext4_dir_destroy_result(&new_dir_ref, &result);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&new_dir_ref);
            put_ext4_inode_ref(&old_dir_ref);
            return -EISDIR;
        } else if (!source_is_dir && target_is_dir) {
            /* Can't overwrite directory with non-directory */
            put_ext4_inode_ref(&target_ref);
            ext4_dir_destroy_result(&new_dir_ref, &result);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&new_dir_ref);
            put_ext4_inode_ref(&old_dir_ref);
            return -ENOTDIR;
        } else if (target_is_dir) {
            /* Both are directories, check if target is empty */
            struct ext4_dir_iter it;
            bool is_empty = true;
            
            ret = ext4_dir_iterator_init(&it, &target_ref, 0);
            if (ret != 0) {
                put_ext4_inode_ref(&target_ref);
                ext4_dir_destroy_result(&new_dir_ref, &result);
                put_ext4_inode_ref(&inode_ref);
                put_ext4_inode_ref(&new_dir_ref);
                put_ext4_inode_ref(&old_dir_ref);
                return ret;
            }
            
            /* Skip "." and ".." entries */
            if (!it.curr) {
                is_empty = true;
            } else {
                /* Skip "." */
                ret = ext4_dir_iterator_next(&it);
                if (ret != 0) {
                    ext4_dir_iterator_fini(&it);
                    put_ext4_inode_ref(&target_ref);
                    ext4_dir_destroy_result(&new_dir_ref, &result);
                    put_ext4_inode_ref(&inode_ref);
                    put_ext4_inode_ref(&new_dir_ref);
                    put_ext4_inode_ref(&old_dir_ref);
                    return ret;
                }
                
                /* Skip ".." */
                ret = ext4_dir_iterator_next(&it);
                if (ret != 0) {
                    ext4_dir_iterator_fini(&it);
                    put_ext4_inode_ref(&target_ref);
                    ext4_dir_destroy_result(&new_dir_ref, &result);
                    put_ext4_inode_ref(&inode_ref);
                    put_ext4_inode_ref(&new_dir_ref);
                    put_ext4_inode_ref(&old_dir_ref);
                    return ret;
                }
                
                /* If there are more entries, the directory is not empty */
                is_empty = (it.curr == NULL);
            }
            
            ext4_dir_iterator_fini(&it);
            
            if (!is_empty) {
                put_ext4_inode_ref(&target_ref);
                ext4_dir_destroy_result(&new_dir_ref, &result);
                put_ext4_inode_ref(&inode_ref);
                put_ext4_inode_ref(&new_dir_ref);
                put_ext4_inode_ref(&old_dir_ref);
                return -ENOTEMPTY;
            }
        }
        
        /* Remove the target */
        ret = ext4_dir_remove_entry(&new_dir_ref, new_dentry->d_name->name, 
                                     new_dentry->d_name->len);
        if (ret != 0) {
            put_ext4_inode_ref(&target_ref);
            ext4_dir_destroy_result(&new_dir_ref, &result);
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&new_dir_ref);
            put_ext4_inode_ref(&old_dir_ref);
            return ret;
        }
        
        /* Decrement link count of target */
        ext4_fs_inode_links_count_dec(&target_ref);
        
        /* Free the inode if it was the last link */
        if (ext4_inode_get_links_cnt(target_ref.inode) == 0) {
            /* Set deletion time */
            ext4_inode_set_del_time(target_ref.inode, time(NULL));
            /* Remove the inode from the filesystem */
            ext4_fs_free_inode(&target_ref);
        }
        
        put_ext4_inode_ref(&target_ref);
        ext4_dir_destroy_result(&new_dir_ref, &result);
    } else if (ret != -ENOENT) {
        /* Error other than "not found" */
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&new_dir_ref);
        put_ext4_inode_ref(&old_dir_ref);
        return ret;
    }
    
    /* Add entry to new directory */
    ret = ext4_dir_add_entry(&new_dir_ref, new_dentry->d_name->name, 
                             new_dentry->d_name->len, &inode_ref);
    if (ret != 0) {
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&new_dir_ref);
        put_ext4_inode_ref(&old_dir_ref);
        return ret;
    }
    
    /* Update parent reference in directory if needed */
    if (S_ISDIR(old_dentry->d_inode->i_mode) && old_dir != new_dir) {
        /* Update ".." entry in the moved directory */
#if CONFIG_DIR_INDEX_ENABLE
        ret = ext4_dir_dx_reset_parent_inode(&inode_ref, new_dir->i_ino);
#else
        /* Manually update the parent inode reference */
        struct ext4_block block;
        ext4_fsblk_t fblock;
        
        /* Get the first block of the directory which contains "." and ".." */
        ret = ext4_fs_get_inode_dblk_idx(&inode_ref, 0, &fblock, false);
        if (ret == 0 && fblock != 0) {
            ret = ext4_block_get(inode_ref.fs->bdev, &block, fblock);
            if (ret == 0) {
                struct ext4_dir_en *dotdot;
                
                /* Get pointer to ".." entry (typically the second entry) */
                dotdot = (struct ext4_dir_en *)((char *)block.data + 
                           ext4_dir_en_get_entry_len((struct ext4_dir_en *)block.data));
                
                /* Update the inode number */
                ext4_dir_en_set_inode(dotdot, new_dir->i_ino);
                
                /* Write the block back */
                ext4_block_set(inode_ref.fs->bdev, &block);
            }
        }
#endif
        if (ret != 0) {
            /* Failed to update parent reference, but entry was already added */
            /* Should probably try to roll back, but for now just report error */
            put_ext4_inode_ref(&inode_ref);
            put_ext4_inode_ref(&new_dir_ref);
            put_ext4_inode_ref(&old_dir_ref);
            return ret;
        }
    }
    
    /* Remove entry from old directory */
    ret = ext4_dir_remove_entry(&old_dir_ref, old_dentry->d_name->name, 
                                old_dentry->d_name->len);
    if (ret != 0) {
        /* Failed to remove from old directory, but already added to new directory */
        put_ext4_inode_ref(&inode_ref);
        put_ext4_inode_ref(&new_dir_ref);
        put_ext4_inode_ref(&old_dir_ref);
        return ret;
    }
    
    /* Update the dentry in VFS */
    dentry_instantiate(new_dentry, inode_get(old_dentry->d_inode));
    
    /* Release references */
    put_ext4_inode_ref(&inode_ref);
    put_ext4_inode_ref(&new_dir_ref);
    put_ext4_inode_ref(&old_dir_ref);
    
    return 0;
}