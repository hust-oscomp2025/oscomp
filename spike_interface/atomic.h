// See LICENSE for license details.
// borrowed from https://github.com/riscv/riscv-pk:
// machine/atomic.h

#ifndef _RISCV_ATOMIC_H_
#define _RISCV_ATOMIC_H_
#include <stdatomic.h>
#include <stdint.h>

// 读取 `sstatus`
static inline uint64_t read_sstatus() {
    uint64_t value;
    asm volatile("csrr %0, sstatus" : "=r"(value));
    return value;
}

// 写入 `sstatus`
static inline void write_sstatus(uint64_t value) {
    asm volatile("csrw sstatus, %0" :: "r"(value));
}

// 禁用 S-mode 中断（清除 SIE 位）
static inline uint64_t disable_irqsave() {
    uint64_t sstatus = read_sstatus();
    write_sstatus(sstatus & ~0x2); // 清除 SIE (bit 1)
    return sstatus;
}

// 恢复 S-mode 中断
static inline void enable_irqrestore(uint64_t flags) {
    write_sstatus(flags);
}


typedef struct {
  atomic_flag  lock;
} spinlock_t;

#define SPINLOCK_INIT  ATOMIC_FLAG_INIT 




static inline int spinlock_trylock(spinlock_t* lock) {
    return atomic_flag_test_and_set(&lock->lock) == 0;  // 成功返回 1
}

static inline void spinlock_lock(spinlock_t* lock) {
    while (atomic_flag_test_and_set(&lock->lock)) {
        // 自旋等待
    }
}

static inline void spinlock_unlock(spinlock_t* lock) {
    atomic_flag_clear(&lock->lock);
}

static inline long spinlock_lock_irqsave(spinlock_t* lock) {
    long flags = disable_irqsave();
    while (atomic_flag_test_and_set(&lock->lock)) {
        // 自旋等待
    }
    return flags;
}
static inline void spinlock_unlock_irqrestore(spinlock_t* lock, long flags) {
    atomic_flag_clear(&lock->lock);
    enable_irqrestore(flags);
}
#endif
