/*
 * The supporting library for applications.
 * Actually, supporting routines for applications are catalogued as the user 
 * library. we don't do that in PKE to make the relationship between application 
 * and user library more straightforward.
 */

#include "user_lib.h"
#include "util/types.h"
#include "util/snprintf.h"
#include "kernel/syscall.h"


uint64 do_user_call(uint64 sysnum, uint64 a1, uint64 a2, uint64 a3, uint64 a4, uint64 a5, uint64 a6,
                 uint64 a7) {
  int ret;

  // before invoking the syscall, arguments of do_user_call are already loaded into the argument
  // registers (a0-a7) of our (emulated) risc-v machine.
  asm volatile(
      "ecall\n"
      "sw a0, %0"  // returns a 32-bit value
      : "=m"(ret)
      :
      : "memory");

  return ret;
}

//
// printu() supports user/lab1_1_helloworld.c
//
int printu(const char* s, ...) {
  va_list vl;
  va_start(vl, s);

  char out[256];  // fixed buffer size.
  int res = vsnprintf(out, sizeof(out), s, vl);
  va_end(vl);


  const char* buf = out;
  size_t n = res < sizeof(out) ? res : sizeof(out);

  // make a syscall to implement the required functionality.
  return do_user_call(SYS_user_print, (uint64)buf, n, 0, 0, 0, 0, 0);
}


//lab1_challenge1
int print_backtrace(int depth){
  return do_user_call(SYS_user_print_backtrace, depth, 0, 0, 0, 0, 0, 0); 
}


void printRegs(){
  printu("========printing reg status========\n");
  uint64 reg_value;
  
  // 输出寄存器的值及别名
  #define PRINT_REG(reg, alias) \
    __asm__ volatile("mv %0, " #reg : "=r"(reg_value)); \
    printu(#alias " = 0x%lx\n", reg_value);

  // 打印常用寄存器并使用别名
  PRINT_REG(x0, zero);
  PRINT_REG(x1, ra);
  PRINT_REG(x2, sp);
  PRINT_REG(x3, gp);
  PRINT_REG(x4, tp);
  PRINT_REG(x5, t0);
  PRINT_REG(x6, t1);
  PRINT_REG(x7, t2);
  PRINT_REG(x8, s0); // s0 或 fp
  PRINT_REG(x9, s1);
  PRINT_REG(x10, a0);
  PRINT_REG(x11, a1);
  PRINT_REG(x12, a2);
  PRINT_REG(x13, a3);
  PRINT_REG(x14, a4);
  PRINT_REG(x15, a5);
  PRINT_REG(x16, a6);
  PRINT_REG(x17, a7);
  PRINT_REG(x28, t3);
  PRINT_REG(x29, t4);
  PRINT_REG(x30, t5);
  PRINT_REG(x31, t6);

  // 打印栈指针（SP）、栈帧指针（FP）
  uint64 sp, fp;
  __asm__ volatile("mv %0, sp" : "=r"(sp));
  __asm__ volatile("mv %0, fp" : "=r"(fp));

  printu("SP = 0x%lx\n", sp);
  printu("FP = 0x%lx\n", fp);
}

// 获取调用 print_registers 的程序计数器（PC）
int getRa(void) {
    uint64 return_address;
    
    // 使用 fp 获取返回地址
    __asm__ volatile (
        "mv %0, fp"           // 将 fp 寄存器的值（栈帧指针）保存到 %0
        : "=r"(return_address) // 输出参数
    );

    // 返回栈帧中的返回地址
    return return_address - 4;  // 获取栈帧中的返回地址
}



int exit(int code) {
  return do_user_call(SYS_user_exit, code, 0, 0, 0, 0, 0, 0); 
}

void* better_malloc(int n) {
  return (void*)do_user_call(SYS_user_malloc, n, 0, 0, 0, 0, 0, 0);
}

void* naive_malloc(int n) {
  return (void*)do_user_call(SYS_user_malloc, 4000, 0, 0, 0, 0, 0, 0);
}

void better_free(void* va) {
  do_user_call(SYS_user_free, (uint64)va, 0, 0, 0, 0, 0, 0);
}

void naive_free(void* va) {
  do_user_call(SYS_user_free, (uint64)va, 0, 0, 0, 0, 0, 0);
}

//
// lib call to naive_fork
int fork() {

  return do_user_call(SYS_user_fork, 0, 0, 0, 0, 0, 0, 0);
}

//
// lib call to yield
//
void yield() {
  do_user_call(SYS_user_yield, 0, 0, 0, 0, 0, 0, 0);
}

//
// lib call to open
//
int open(const char *pathname, int flags) {
  return do_user_call(SYS_user_open, (uint64)pathname, flags, 0, 0, 0, 0, 0);
}

//
// lib call to read
//
int read_u(int fd, void * buf, uint64 count){
  return do_user_call(SYS_user_read, fd, (uint64)buf, count, 0, 0, 0, 0);
}

//
// lib call to write
//
int write_u(int fd, void *buf, uint64 count) {
  return do_user_call(SYS_user_write, fd, (uint64)buf, count, 0, 0, 0, 0);
}

//
// lib call to seek
// 
int lseek_u(int fd, int offset, int whence) {
  return do_user_call(SYS_user_lseek, fd, offset, whence, 0, 0, 0, 0);
}

//
// lib call to read file information
//
int stat_u(int fd, struct istat *istat) {
  return do_user_call(SYS_user_stat, fd, (uint64)istat, 0, 0, 0, 0, 0);
}

//
// lib call to read file information from disk
//
int disk_stat_u(int fd, struct istat *istat) {
  return do_user_call(SYS_user_disk_stat, fd, (uint64)istat, 0, 0, 0, 0, 0);
}




//
// lib call to open dir
//
int opendir_u(const char *dirname) {
  return do_user_call(SYS_user_opendir, (uint64)dirname, 0, 0, 0, 0, 0, 0);
}

//
// lib call to read dir
//
int readdir_u(int fd, struct dir *dir) {
  return do_user_call(SYS_user_readdir, fd, (uint64)dir, 0, 0, 0, 0, 0);
}

//
// lib call to make dir
//
int mkdir_u(const char *pathname) {
  return do_user_call(SYS_user_mkdir, (uint64)pathname, 0, 0, 0, 0, 0, 0);
}

//
// lib call to close dir
//
int closedir_u(int fd) {
  return do_user_call(SYS_user_closedir, fd, 0, 0, 0, 0, 0, 0);
} 

//
// lib call to link
//
int link_u(const char *fn1, const char *fn2){
  return do_user_call(SYS_user_link, (uint64)fn1, (uint64)fn2, 0, 0, 0, 0, 0);
}

//
// lib call to unlink
//
int unlink_u(const char *fn){
  return do_user_call(SYS_user_unlink, (uint64)fn, 0, 0, 0, 0, 0, 0);
}

//
// lib call to close
//
int close(int fd) {
  return do_user_call(SYS_user_close, fd, 0, 0, 0, 0, 0, 0);
}

//
// lib call to read present working directory (pwd)
//
int read_cwd(char *path) {
  return do_user_call(SYS_user_rcwd, (uint64)path, 0, 0, 0, 0, 0, 0);
}

//
// lib call to change pwd
//
int change_cwd(const char *path) {
  return do_user_call(SYS_user_ccwd, (uint64)path, 0, 0, 0, 0, 0, 0);
}


int wait(int pid){
  return do_user_call(SYS_user_wait,(uint64)pid,0,0,0,0,0,0);
}

void test_kernel(void){
  do_user_call(SYS_user_test,0,0,0,0,0,0,0);
}

int sem_new(int n) {
    return do_user_call(SYS_user_sem_new, n, 0, 0, 0, 0, 0, 0);
}
//
// lib call to sem_P
//
int sem_P(int n) {
    return do_user_call(SYS_user_sem_P, n, 0, 0, 0, 0, 0, 0);
}

int sem_V(int n) {
    return do_user_call(SYS_user_sem_V, n, 0, 0, 0, 0, 0, 0);
}

int exec(const char* path){
	return do_user_call(SYS_user_exec, (uint64)path, 0, 0, 0, 0, 0, 0);
}


void printpa(int* va){
  do_user_call(SYS_user_printpa, (uint64)va, 0, 0, 0, 0, 0, 0);
}