/*
 * contains the implementation of all syscalls.
 */

#include <errno.h>
#include <stdint.h>

#include <kernel/elf.h>

#include <kernel/pmm.h>
#include <kernel/proc_file.h>
#include <kernel/process.h>
#include <kernel/sched.h>

#include <spike_interface/spike_utils.h>

#include <kernel/syscall.h>
#include <kernel/types.h>
#include <kernel/vmm.h>
#include <util/string.h>

#include <kernel/semaphore.h>
#include <kernel/sync_utils.h>

//
// implement the SYS_user_print syscall
//
ssize_t sys_user_print(const char *buf, size_t n) {
  int hartid = read_tp();
  // buf is now an address in user space of the given app's user stack,
  // so we have to transfer it into phisical address (kernel is running in
  // direct mapping).
  assert(CURRENT);
  char *pa =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)buf);
  sprint("%s\n", pa);
  return 0;
}

//
// implement the SYS_user_exit syscall
//

volatile static int counter = 0;
ssize_t sys_user_exit(uint64 code) {
  int hartid = read_tp();
  sprint("User exit with code:%d.\n", code);
  CURRENT->status = ZOMBIE;
  sem_V(CURRENT->sem_index);
  if (CURRENT->parent != NULL)
    sem_V(CURRENT->parent->sem_index);

  // reclaim the current process, and reschedule. added @lab3_1
  // 这个过程只在父进程显式调用wait时执行。如果说父进程提前退出，那么子进程应该被init进程收养。
  // free_process(current);
  schedule();

  return 0;
}

uint64 sys_user_malloc(size_t size) { return (uint64)vmalloc(size); }

//
// reclaim a page, indicated by "va". added @lab2_2
//
uint64 sys_user_free(uint64 va) {
  // user_vm_unmap((pagetable_t)current->pagetable, va, PGSIZE, 1);
  free((void *)va);
  return 0;
}

ssize_t sys_user_print_backtrace(uint64 depth) {

  if (depth <= 0)
    return 0;
  int hartid = read_tp();
  trapframe *tf = CURRENT->trapframe;
  uint64 temp_fp = tf->regs.s0;
  uint64 temp_pc = tf->epc;

  temp_fp = *(uint64 *)user_va_to_pa((pagetable_t)(CURRENT->pagetable),
                                     (void *)temp_fp - 16);
  for (int i = 1; i <= depth; i++) {
    temp_pc = *(uint64 *)user_va_to_pa((pagetable_t)(CURRENT->pagetable),
                                       (void *)temp_fp - 8);
    char *function_name = locate_function_name(temp_pc);
    sprint("%s\n", function_name);
    if (strcmp(function_name, "main") == 0) {
      return i;
    } else {
      temp_fp = *(uint64 *)user_va_to_pa((pagetable_t)(CURRENT->pagetable),
                                         (void *)temp_fp - 16);
    }
  }
  return depth;
}

ssize_t sys_user_fork() {
  sprint("User call fork.\n");
  return do_fork(CURRENT);
}

ssize_t sys_user_wait(int pid) { return do_wait(pid); }

int sys_user_sem_new(int initial_value) {
  // int pid = current->pid;
  return sem_new(initial_value);
}

int sys_user_sem_P(int sem_index) { return sem_P(sem_index); }

int sys_user_sem_V(int sem_index) {
  int hartid = read_tp();
  return sem_V(sem_index);
}

ssize_t sys_user_yield() {
  // TODO (lab3_2): implment the syscall of yield.
  // hint: the functionality of yield is to give up the processor. therefore,
  // we should set the status of currently running process to READY, insert it
  // in the rear of ready queue, and finally, schedule a READY process to run.
  // panic( "You need to implement the yield syscall in lab3_2.\n" );
  int hartid = read_tp();
  // current->status = READY;
  insert_to_ready_queue(CURRENT);
  schedule();
  return 0;
}

ssize_t sys_user_printpa(uint64 va) {
  uint64 pa =
      (uint64)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)va);
  sprint("%lx\n", pa);
  return 0;
}

//
// open file
//
ssize_t sys_user_open(char *pathva, int flags) {
  char *pathpa =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), pathva);
  return do_open(pathpa, flags);
}

//
// read file
//
ssize_t sys_user_read(int fd, char *bufva, uint64 count) {
  int i = 0;
  while (i < count) { // count can be greater than page size
    uint64 addr = (uint64)bufva + i;
    uint64 pa = lookup_pa((pagetable_t)CURRENT->pagetable, addr);
    uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
    uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
    uint64 r = do_read(fd, (char *)pa + off, len);
    i += r;
    if (r < len)
      return i;
  }
  return count;
}

//
// write file
//
ssize_t sys_user_write(int fd, char *bufva, uint64 count) {
  int i = 0;
  while (i < count) { // count can be greater than page size
    uint64 addr = (uint64)bufva + i;
    uint64 pa = lookup_pa((pagetable_t)CURRENT->pagetable, addr);
    uint64 off = addr - ROUNDDOWN(addr, PGSIZE);
    uint64 len = count - i < PGSIZE - off ? count - i : PGSIZE - off;
    uint64 r = do_write(fd, (char *)pa + off, len);
    i += r;
    if (r < len)
      return i;
  }
  return count;
}

//
// lseek file
//
ssize_t sys_user_lseek(int fd, int offset, int whence) {
  return do_lseek(fd, offset, whence);
}

//
// read vinode
//
ssize_t sys_user_stat(int fd, struct istat *istat) {
  struct istat *pistat =
      (struct istat *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), istat);
  return do_stat(fd, pistat);
}

//
// read disk inode
//
ssize_t sys_user_disk_stat(int fd, struct istat *istat) {
  struct istat *pistat =
      (struct istat *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), istat);
  return do_stat(fd, pistat);
}

//
// close file
//
ssize_t sys_user_close(int fd) { return do_close(fd); }

//
// lib call to opendir
//
ssize_t sys_user_opendir(char *pathva) {
  char *pathpa =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), pathva);
  return do_opendir(pathpa);
}

//
// lib call to readdir
//
ssize_t sys_user_readdir(int fd, struct dir *vdir) {
  struct dir *pdir =
      (struct dir *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), vdir);
  return do_readdir(fd, pdir);
}

//
// lib call to mkdir
//
ssize_t sys_user_mkdir(char *pathva) {
  char *pathpa =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), pathva);
  return do_mkdir(pathpa);
}

//
// lib call to closedir
//
ssize_t sys_user_closedir(int fd) { return do_closedir(fd); }

//
// lib call to link
//
ssize_t sys_user_link(char *vfn1, char *vfn2) {
  char *pfn1 =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vfn1);
  char *pfn2 =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vfn2);
  return do_link(pfn1, pfn2);
}

//
// lib call to unlink
//
ssize_t sys_user_unlink(char *vfn) {
  char *pfn =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vfn);
  return do_unlink(pfn);
}

ssize_t sys_user_rcwd(char *vpath) {
  char *path =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vpath);
  return do_rcwd(path);
}

ssize_t sys_user_ccwd(char *vpath) {
  char *path =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vpath);
  return do_ccwd(path);
}

ssize_t sys_user_exec(char *vpath) {
  char *path =
      (char *)user_va_to_pa((pagetable_t)(CURRENT->pagetable), (void *)vpath);
  // we shall never reach here
  return do_exec(path);
  ;
}

//
// [a0]: the syscall number; [a1] ... [a7]: arguments to the syscalls.
// returns the code of success, (e.g., 0 means success, fail for otherwise)
//
long do_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long a6,
                long a7) {
  sprint("do_syscall: syscall number: %ld\n", a7);
  sprint("Arguments: %ld, %ld, %ld, %ld, %ld, %ld, %ld\n", a0, a1, a2, a3, a4,
         a5, a6);
	syscall_function *sys_func = &sys_table[a7];
	uint64_t (*func)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
	func = (uint64_t(*)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t))sys_func->func;
	return func(a0, a1, a2, a3, a4, a5);
}
