// See LICENSE for license details.
// borrowed from https://github.com/riscv/riscv-pk:
// machine/atomic.h

#ifndef _RISCV_SPINLOCK_H_
#define _RISCV_SPINLOCK_H_
#include <stdatomic.h>
#include <stdint.h>
#include <kernel/atomic.h>



typedef struct {
  atomic_flag lock;
} spinlock_t;

#define SPINLOCK_INIT  ATOMIC_FLAG_INIT 

static inline void spinlock_init(spinlock_t* lock) {
	atomic_flag_clear(&lock->lock);
}





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

static inline void spinlock_unlock_irqrestore(spinlock_t* lock, long flags) {
    atomic_flag_clear(&lock->lock);
    enable_irqrestore(flags);
}

static inline long spinlock_lock_irqsave(spinlock_t* lock) {
	long flags = disable_irqsave();
	while (atomic_flag_test_and_set(&lock->lock)) {
			// 自旋等待
	}
	return flags;
}

#endif
