#ifndef _VMM_H_
#define _VMM_H_

#include <kernel/riscv.h>
#include <kernel/process.h>

// permission codes.
enum VMPermision {
  PROT_NONE = 0,
  PROT_READ = 1,
  PROT_WRITE = 2,
  PROT_EXEC = 4,
};


// 堆内存管理
typedef struct heap_block_t {
    size_t size;                  // 堆块大小（包含元数据）
    struct heap_block_t * prev;      // 前一个空闲块指针
    struct heap_block_t * next;      // 后一个空闲块指针
    int free;                     // 标志位，表示是否为空闲块
} heap_block;

uint64 prot_to_type(int prot, int user);

/* --- kernel page table --- */


// Initialize the kernel pagetable
void kern_vm_init(void);


#endif
