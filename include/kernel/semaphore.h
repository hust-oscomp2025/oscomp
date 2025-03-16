#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

#include <kernel/sched/process.h>

#define NSEM 256

typedef struct semaphore_t {
  int isActive;
  int value;
  struct task_struct *wait_queue;
  int pid; // 系统信号量为-1
} semaphore;

int sem_new(int initial_value);
void sem_free(int sem_index);
int sem_P(int sem_index);
int sem_V(int sem_index);


#endif