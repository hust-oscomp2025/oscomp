#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <kernel/sched/process.h>

#define NSEM 256

typedef struct semaphore_t {
  int32 isActive;
  int32 value;
  struct task_struct *wait_queue;
  int32 pid; // 系统信号量为-1
} semaphore;

int32 sem_new(int32 initial_value);
void sem_free(int32 sem_index);
int32 sem_P(int32 sem_index);
int32 sem_V(int32 sem_index);


#endif