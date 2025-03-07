#include <kernel/file.h>

#include <kernel/pmm.h>

//
// alloc a (virtual) file
//
struct file *alloc_vfs_file(struct dentry *file_dentry, int readable,
                            int writable, int offset) {
  struct file *file = alloc_page();
  file->f_dentry = file_dentry;
  file_dentry->d_ref += 1;
  file->f_mode = 0;
  if (readable) {
    file->f_mode |=FMODE_READ;
  }if(writable){
		file->f_mode |= FMODE_WRITE;
	}
  file->f_pos = 0;
  return file;
}