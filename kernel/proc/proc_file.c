/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include <kernel/proc_file.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/process.h>
#include <kernel/riscv.h>
#include "spike_interface/spike_file.h"
#include <spike_interface/spike_utils.h>

#include <util/string.h>



//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)kmalloc(sizeof(proc_file_management));
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->fd_table[fd] = NULL;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void free_proc_file_management(proc_file_management *pfiles) {
	kfree(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
	if(fd < 0 || fd >= MAX_FILES){
		panic("do_read: invalid fd!\n");
	}
  struct file *pfile = CURRENT->pfiles->fd_table[fd]; // file entry
	if (pfile == NULL)
		panic("do_read: no such opened file!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
	sprint("do_open: begin.\n");
  struct file *opened_file = NULL;
  if ((opened_file = vfs_open(pathname, flags)) == NULL)
    return -1;


	sprint("do_open: allocating fd.\n");

	// 从进程控制块中分配fd
  for (int fd = 0; fd < MAX_FILES; ++fd) {
    struct file *pfile = CURRENT->pfiles->fd_table[fd];
    if (pfile == NULL) {
      // initialize this file structure
			pfile = (struct file *)kmalloc(sizeof(struct file));
      memcpy(pfile, opened_file, sizeof(struct file));
      CURRENT->pfiles->nfiles++;
      return fd;
    }
  }
  panic("do_open: no file entry for current process!\n");
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if ( (pfile->f_mode & FMODE_READ) == 0)
    panic("do_read: no readable file!\n");

  char buffer[count + 1];
  int len = vfs_read(pfile, buffer, count);
  buffer[count] = '\0';
  strcpy(buf, buffer);
  return len;
}

//
// write content ("buf") whose length is "count" to a file "fd".
// return: actual length of data written to the file.
//
int do_write(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (!(pfile->f_mode & FMODE_WRITE)) 
    panic("do_write: cannot write file!\n");

  int len = vfs_write(pfile, buf, count);
  return len;
}

//
// reposition the file offset
//
int do_lseek(int fd, int offset, int whence) {
  struct file *pfile = get_opened_file(fd);
  return vfs_lseek(pfile, offset, whence);
}

//
// read the vinode information
//
int do_fstat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_stat(int fd, struct istat *istat) {
	
  struct file *pfile = get_opened_file(fd);
	if(pfile){
		return vfs_disk_stat(pfile, istat);
	}else{
		sprint("empty fd\n");
		return -1;
	}
  
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
	int ret = vfs_close(pfile);
	kfree(pfile);
	CURRENT->pfiles->fd_table[fd] = NULL;
	CURRENT->pfiles->nfiles--;
  return ret;
}

//
// open a directory
// return: the fd of the directory file
//
int do_opendir(char *pathname) {
  struct file *opened_file = vfs_opendir(pathname);
  if (opened_file == NULL)
    return -1;

	// 从进程控制块中分配fd
  for (int fd = 0; fd < MAX_FILES; ++fd) {
    struct file *pfile = CURRENT->pfiles->fd_table[fd];
    if (pfile == NULL) {
      // initialize this file structure
			pfile = (struct file *)kmalloc(sizeof(struct file));
      memcpy(pfile, opened_file, sizeof(struct file));
      CURRENT->pfiles->nfiles++;
      return fd;
    }
  }
  panic("do_opendir: no file entry for current process!\n");
}

//
// read a directory entry
//
int do_readdir(int fd, struct dir *dir) {
  struct file *pfile = get_opened_file(fd);
  return vfs_readdir(pfile, dir);
}

//
// make a new directory
//
int do_mkdir(char *pathname) { return vfs_mkdir(pathname); }

//
// close a directory
//
int do_closedir(int fd) {
  struct file *pfile = get_opened_file(fd);
  return vfs_closedir(pfile);
}

//
// create hard link to a file
//
int do_link(char *oldpath, char *newpath) {
  return vfs_link(oldpath, newpath);
}

//
// remove a hard link to a file
//
int do_unlink(char *path) {
  return vfs_unlink(path);
}

//
// Get the absolute path of current working directory
// and copy it to the provided buffer
//
ssize_t do_rcwd(char* path) {
    if (!path) {
        return -1;  // Invalid buffer
    }
    
    struct dentry* cwd = CURRENT->pfiles->cwd;
    if (!cwd) {
        return -1;  // No current working directory
    }
    
    // Special case: root directory
    if (cwd == vfs_root_dentry) {
        strcpy(path, "/");
        return 0;
    }
    
    // Build path by traversing up the dentry tree
    char temp_path[MAX_PATH_LEN];
    temp_path[0] = '\0';  // Initialize as empty string
    
    struct dentry* current_dentry = cwd;
    int total_length = 0;
    
    // Traverse up until we reach the root
    while (current_dentry && current_dentry != vfs_root_dentry) {
        // Check if the path would exceed the buffer
        int name_len = strlen(current_dentry->name);
        total_length += name_len + 1;  // +1 for the '/'
        
        if (total_length >= MAX_PATH_LEN) {
            // Path too long
            path[0] = '\0';
            return -1;
        }
        
        // Prepend this component to the path
        char temp_buffer[MAX_PATH_LEN];
        strcpy(temp_buffer, "/");
        strcat(temp_buffer, current_dentry->name);
        strcat(temp_buffer, temp_path);
        
        strcpy(temp_path, temp_buffer);
        
        // Move up to parent
        current_dentry = current_dentry->parent;
    }
    
    // If the path is empty (should not happen), return root
    if (temp_path[0] == '\0') {
        strcpy(path, "/");
    } else {
        strcpy(path, temp_path);
    }
    
    return 0;
}


ssize_t do_ccwd(char* path) {
	struct file* dir_file = vfs_opendir(path);
	if (dir_file == NULL)
    return -1;
	CURRENT->pfiles->cwd = dir_file->f_dentry;
	return 0;
}

static void release_proc_files(proc_file_management* pfiles) {
	for (int i = 0; i < MAX_FILES; i++) {
		if (pfiles->fd_table[i]) {
			do_close(i);
		}
	}
}

