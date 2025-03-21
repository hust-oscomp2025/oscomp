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

