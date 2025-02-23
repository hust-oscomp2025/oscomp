/*
 * header file to be used by applications.
 */

int printu(const char *s, ...);
int exit(int code);
void printRegs();
int getRa(void);
int print_backtrace(int depth);
void* better_malloc(int n);
void better_free(void* va);
void naive_free(void* va);
void* naive_malloc();
int fork();
void yield();
void wait(int pid);
void test_kernel(void);

int sem_new(int initial_value);
int sem_P(int sem_index);

int sem_V(int sem_index);