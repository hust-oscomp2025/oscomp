#ifndef PID_H
#define PID_H

#include <stdint.h>
#include <kernel/util/spinlock.h>    // 自旋锁支持
#include <kernel/types.h>

#define PID_MAX 8192    // 最大PID值


struct pid_manager {
    spinlock_t lock;   // 自旋锁
    int32_t next_pid;          // 下一个待分配的PID
    uint8_t pid_map[PID_MAX/8];// PID位图（含位操作对齐）
};

// 初始化PID管理器（需检查返回值）
void pid_init();

// 分配PID（返回错误码，pid通过指针输出）
pid_t pid_alloc(void);

// 释放PID（返回错误码）
void pid_free(pid_t pid);

#endif