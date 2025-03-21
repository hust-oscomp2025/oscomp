#include <kernel/fs/file.h>
#include <kernel/fs/inode.h>
#include <kernel/fs/kiocb.h>
#include <kernel/fs/vfs.h>
#include <kernel/types.h>
#include <util/string.h>

/**
 * init_kiocb - Initialize a kernel I/O control block
 * @kiocb: The kiocb to initialize
 * @file: The file this I/O operation is for
 */
void init_kiocb(struct kiocb *kiocb, struct file *file) {
    memset(kiocb, 0, sizeof(*kiocb));
    kiocb->ki_filp = file;
    kiocb->ki_pos = file->f_pos;
}

/**
 * setup_io_vector_iterator - Initialize an I/O vector iterator
 * @iter: The iterator to initialize
 * @vec: Array of io_vector structures
 * @vlen: Number of io_vector structures
 *
 * Returns total size of all io_vector segments or negative error
 */
int setup_io_vector_iterator(struct io_vector_iterator *iter, const struct io_vector *vec, 
                   unsigned long vlen) {
    size_t total_size = 0;
    
    if (!iter || !vec || vlen == 0)
        return -EINVAL;
    
    iter->iovec = (struct io_vector *)vec;
    iter->nr_segs = vlen;
    iter->iov_offset = 0;
    
    /* Calculate total bytes */
    for (unsigned long i = 0; i < vlen; i++) {
        if (!vec[i].iov_base && vec[i].iov_len > 0)
            return -EFAULT;  /* Invalid buffer */
        total_size += vec[i].iov_len;
    }
    
    iter->count = total_size;
    return total_size;
}

/**
 * vfs_readv - Read data from a file into multiple buffers
 * @file: File to read from
 * @vec: Array of io_vector structures
 * @vlen: Number of io_vector structures
 * @pos: Position in file to read from (updated on return)
 */
ssize_t vfs_readv(struct file *file, const struct io_vector *vec, 
                  unsigned long vlen, loff_t *pos) {
    struct kiocb kiocb;
    struct io_vector_iterator iter;
    ssize_t ret;
    
    if (!file || !vec || !pos)
        return -EINVAL;
    
    if (!(file->f_mode & FMODE_READ))
        return -EBADF;
    
    init_kiocb(&kiocb, file);
    kiocb.ki_pos = *pos;
    
    ret = setup_io_vector_iterator(&iter, vec, vlen);
    if (ret < 0)
        return ret;
    
    /* Use optimized read_iter if available */
    if (file->f_operations && file->f_operations->read_iter) {
        ret = file->f_operations->read_iter(&kiocb, &iter);
    } else if (file->f_operations && file->f_operations->read) {
        /* Fall back to sequential reads */
        ret = 0;
        for (unsigned long i = 0; i < vlen; i++) {
            ssize_t bytes;
            bytes = file->f_operations->read(file, vec[i].iov_base, 
                                           vec[i].iov_len, &kiocb.ki_pos);
            if (bytes < 0) {
                if (ret == 0)
                    ret = bytes;
                break;
            }
            ret += bytes;
            if (bytes < vec[i].iov_len)
                break;  /* Short read */
        }
    } else {
        ret = -EINVAL;
    }
    
    if (ret > 0)
        *pos = kiocb.ki_pos;
    
    return ret;
}

/**
 * vfs_writev - Write data from multiple buffers to a file
 * @file: File to write to
 * @vec: Array of io_vector structures
 * @vlen: Number of io_vector structures
 * @pos: Position in file to write to (updated on return)
 */
ssize_t vfs_writev(struct file *file, const struct io_vector *vec, 
                   unsigned long vlen, loff_t *pos) {
    struct kiocb kiocb;
    struct io_vector_iterator iter;
    ssize_t ret;
    
    if (!file || !vec || !pos)
        return -EINVAL;
    
    if (!(file->f_mode & FMODE_WRITE))
        return -EBADF;
    
    init_kiocb(&kiocb, file);
    kiocb.ki_pos = *pos;
    
    ret = setup_io_vector_iterator(&iter, vec, vlen);
    if (ret < 0)
        return ret;
    
    if (file->f_operations && file->f_operations->write_iter) {
        ret = file->f_operations->write_iter(&kiocb, &iter);
    } else if (file->f_operations && file->f_operations->write) {
        ret = 0;
        for (unsigned long i = 0; i < vlen; i++) {
            ssize_t bytes;
            bytes = file->f_operations->write(file, vec[i].iov_base, 
                                            vec[i].iov_len, &kiocb.ki_pos);
            if (bytes < 0) {
                if (ret == 0)
                    ret = bytes;
                break;
            }
            ret += bytes;
            if (bytes < vec[i].iov_len)
                break;  /* Short write */
        }
    } else {
        ret = -EINVAL;
    }
    
    if (ret > 0) {
        *pos = kiocb.ki_pos;
        mark_inode_dirty(file->f_inode);
    }
    
    return ret;
}