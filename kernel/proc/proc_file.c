/*
 * Interface functions between file system and kernel/processes. added @lab4_1
 */

#include <kernel/proc_file.h>


#include <kernel/hostfs.h>
#include <kernel/pmm.h>
#include <kernel/process.h>
#include <kernel/ramdev.h>
#include <kernel/rfs.h>
#include <kernel/riscv.h>
#include "spike_interface/spike_file.h"
#include "spike_interface/spike_utils.h"
#include "util/functions.h"
#include <util/string.h>

//
// initialize file system
//
void fs_init(void) {
  // initialize the vfs
  vfs_init();

  // register hostfs and mount it as the root
  if (register_hostfs() < 0)
    panic("fs_init: cannot register hostfs.\n");
  struct device *hostdev = init_host_device("HOSTDEV");
  vfs_mount("HOSTDEV", MOUNT_AS_ROOT);

  // register and mount rfs
  if (register_rfs() < 0)
    panic("fs_init: cannot register rfs.\n");
  struct device *ramdisk0 = init_rfs_device("RAMDISK0");
  rfs_format_dev(ramdisk0);
  vfs_mount("RAMDISK0", MOUNT_DEFAULT);
}

//
// initialize a proc_file_management data structure for a process.
// return the pointer to the page containing the data structure.
//
proc_file_management *init_proc_file_management(void) {
  proc_file_management *pfiles = (proc_file_management *)alloc_page();
  pfiles->cwd = vfs_root_dentry; // by default, cwd is the root
  pfiles->nfiles = 0;

  for (int fd = 0; fd < MAX_FILES; ++fd)
    pfiles->opened_files[fd].status = FD_NONE;

  sprint("FS: created a file management struct for a process.\n");
  return pfiles;
}

//
// reclaim the open-file management data structure of a process.
// note: this function is not used as PKE does not actually reclaim a process.
//
void reclaim_proc_file_management(proc_file_management *pfiles) {
  free_page(pfiles);
  return;
}

//
// get an opened file from proc->opened_file array.
// return: the pointer to the opened file structure.
//
struct file *get_opened_file(int fd) {
  struct file *pfile = NULL;

  // browse opened file list to locate the fd
  for (int i = 0; i < MAX_FILES; ++i) {
    pfile = &(current[read_tp()]->pfiles->opened_files[i]); // file entry
    if (i == fd)
      break;
  }
  if (pfile == NULL)
    panic("do_read: invalid fd!\n");
  return pfile;
}

//
// open a file named as "pathname" with the permission of "flags".
// return: -1 on failure; non-zero file-descriptor on success.
//
int do_open(char *pathname, int flags) {
  struct file *opened_file = NULL;
  if ((opened_file = vfs_open(pathname, flags)) == NULL)
    return -1;

  int fd = 0;
  if (current[read_tp()]->pfiles->nfiles >= MAX_FILES) {
    panic("do_open: no file entry for current process!\n");
  }
  struct file *pfile;
  for (fd = 0; fd < MAX_FILES; ++fd) {
    pfile = &(current[read_tp()]->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE)
      break;
  }

  // initialize this file structure
  memcpy(pfile, opened_file, sizeof(struct file));

  ++current[read_tp()]->pfiles->nfiles;
  return fd;
}

//
// read content of a file ("fd") into "buf" for "count".
// return: actual length of data read from the file.
//
int do_read(int fd, char *buf, uint64 count) {
  struct file *pfile = get_opened_file(fd);

  if (pfile->readable == 0)
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

  if (pfile->writable == 0)
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
int do_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_stat(pfile, istat);
}

//
// read the inode information on the disk
//
int do_disk_stat(int fd, struct istat *istat) {
  struct file *pfile = get_opened_file(fd);
  return vfs_disk_stat(pfile, istat);
}

//
// close a file
//
int do_close(int fd) {
  struct file *pfile = get_opened_file(fd);
	int ret = vfs_close(pfile);
	pfile->status = FD_NONE;
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
    struct file *pfile = &(current[read_tp()]->pfiles->opened_files[fd]);
    if (pfile->status == FD_NONE) {
      // initialize this file structure
      memcpy(pfile, opened_file, sizeof(struct file));
      current[read_tp()]->pfiles->nfiles++;
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
    
    struct dentry* cwd = current[read_tp()]->pfiles->cwd;
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
	current[read_tp()]->pfiles->cwd = dir_file->f_dentry;
	return 0;
}

static void release_proc_files(proc_file_management* pfiles) {
	for (int i = 0; i < MAX_FILES; i++) {
		if (pfiles->opened_files[i].status != FD_NONE) {
			do_close(i);
		}
	}
}

