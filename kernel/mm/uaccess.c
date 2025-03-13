#include <kernel/mm/mm_struct.h>
#include <kernel/sched/sched.h>
#include <kernel/sched/process.h>
#include <kernel/types.h>


static inline void pagefault_disable(void);
static inline void pagefault_enable(void);
/**
 * copy_to_user - Copy a block of data into user space.
 * @to:   Destination address, in user space.
 * @from: Source address, in kernel space.
 * @n:    Number of bytes to copy.
 *
 * Returns number of bytes that could not be copied.
 * On success, this will be zero. On partial success, this will be non-zero.
 */
unsigned long copy_to_user(void* to, const void* from, unsigned long n)
{
    struct mm_struct *mm = CURRENT->mm;
    ssize_t ret;
    
    // if (!access_ok(to, n))
    //     return n;
    
    pagefault_disable();
    ret = mm_copy_to_user(mm, to, from, n);
    pagefault_enable();
    
    if (ret < 0)
        return n;  // On error, all bytes failed to copy
    
    return n - ret; // Return bytes NOT copied (0 means success)
}


// Implementation
static inline void pagefault_disable(void) { CURRENT->pagefault_disabled++; }

static inline void pagefault_enable(void) {
  if (CURRENT->pagefault_disabled > 0)
    CURRENT->pagefault_disabled--;
}