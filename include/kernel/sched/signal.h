#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <kernel/types.h>

/* Standard POSIX signal numbers */
#define SIGHUP     1  // Hangup
#define SIGINT     2  // Terminal interrupt
#define SIGQUIT    3  // Terminal quit
#define SIGILL     4  // Illegal instruction
#define SIGTRAP    5  // Trace trap
#define SIGABRT    6  // Process abort
#define SIGIOT     SIGABRT // Compatibility 
#define SIGBUS     7  // Bus error
#define SIGFPE     8  // Floating-point exception
#define SIGKILL    9  // Kill (cannot be caught or ignored)
#define SIGUSR1    10 // User-defined signal 1
#define SIGSEGV    11 // Segmentation violation
#define SIGUSR2    12 // User-defined signal 2
#define SIGPIPE    13 // Broken pipe
#define SIGALRM    14 // Alarm clock
#define SIGTERM    15 // Termination
#define SIGSTKFLT  16 // Stack fault
#define SIGCHLD    17 // Child status changed
#define SIGCONT    18 // Continue
#define SIGSTOP    19 // Stop, cannot be caught or ignored
#define SIGTSTP    20 // Keyboard stop
#define SIGTTIN    21 // Background read from tty
#define SIGTTOU    22 // Background write to tty
#define SIGURG     23 // Urgent condition on socket
#define SIGXCPU    24 // CPU time limit exceeded
#define SIGXFSZ    25 // File size limit exceeded
#define SIGVTALRM  26 // Virtual timer expired
#define SIGPROF    27 // Profiling timer expired
#define SIGWINCH   28 // Window size change
#define SIGIO      29 // I/O now possible
#define SIGPWR     30 // Power failure
#define SIGSYS     31 // Bad system call
#define _NSIG      64 // Maximum signal number

/* Number of supported signals */
#define NSIG       (_NSIG + 1)


// 使用posix标准的信号集
// #define _SIGSET_NWORDS (1024 / (8 * sizeof (uint64 int32)))
// typedef struct
// {
//   uint64 int32 __val[_SIGSET_NWORDS];
// } __sigset_t;
// typedef __sigset_t sigset_t;
// 


/* Special handlers */
#define SIG_DFL ((void (*)(int32))0)   /* Default handler */
#define SIG_IGN ((void (*)(int32))1)   /* Ignore signal */
#define SIG_ERR ((void (*)(int32))-1)  /* Error return */

/* Signal action flags */
#define SA_NOCLDSTOP 0x00000001 /* Don't send SIGCHLD when children stop */
#define SA_NOCLDWAIT 0x00000002 /* Don't create zombie on child death */
#define SA_SIGINFO   0x00000004 /* Extended signal handling */
#define SA_ONSTACK   0x08000000 /* Signal delivery on alternate stack */
#define SA_RESTART   0x10000000 /* Restart syscall on signal return */
#define SA_NODEFER   0x40000000 /* Don't mask the signal during handler */
#define SA_RESETHAND 0x80000000 /* Reset handler to SIG_DFL upon delivery */


/* Signal value */
union sigval {
    int32    sival_int;   /* Integer value */
    void  *sival_ptr;   /* Pointer value */
};


/* Additional info about the signal */
typedef struct siginfo {
    int32      si_signo;  /* Signal number */
    int32      si_code;   /* Signal code */
    int32      si_errno;  /* Errno value */
    pid_t    si_pid;    /* Sending process ID */
    uid_t    si_uid;    /* Real user ID of sending process */
    void    *si_addr;   /* Memory address that caused fault */
    int32      si_status; /* Exit value or signal */
    int64     si_band;   /* Band event */
    union sigval si_value; /* Signal value */
} siginfo_t;



/* Signal action structure */
struct sigaction {
    union {
        void (*sa_handler)(int32);             /* Traditional handler */
        void (*sa_sigaction)(int32, siginfo_t *, void *); /* New handler with context */
    };
    sigset_t sa_mask;   /* Signals to block during handler */
    int32      sa_flags;  /* Signal action flags */
    void    (*sa_restorer)(void); /* Obsolete field for ABI compatibility */
};

/* Stack for alternate signal handling */
typedef struct sigaltstack {
    void *ss_sp;     /* Stack base or pointer */
    int32   ss_flags;  /* Flags */
    size_t ss_size;  /* Stack size */
} stack_t;

/* Signal operations - methods for sigprocmask() */
#define SIG_BLOCK     0 /* Block signals in set */
#define SIG_UNBLOCK   1 /* Unblock signals in set */
#define SIG_SETMASK   2 /* Set mask to the provided set */

/* Notification types */
#define SIGEV_NONE   0  /* No notification */
#define SIGEV_SIGNAL 1  /* Notify via signal */
#define SIGEV_THREAD 2  /* Notify via thread creation */

/**
 * Simplified signal event structure for notification preferences
 * Used primarily with timers and asynchronous I/O
 */
struct sigevent {
    int32         sigev_notify;           /* Notification method */
    int32         sigev_signo;            /* Signal number */
    union sigval sigev_value;           /* Data passed with notification */
    
    /* Function to run for SIGEV_THREAD (can be NULL if not using threads) */
    void      (*sigev_notify_function)(union sigval);
    
    /* Thread attributes for SIGEV_THREAD (can be NULL) */
    void       *sigev_notify_attributes;
};




/* Core signal operations */
int32 do_send_signal(pid_t pid, int32 sig);
int32 do_sigaction(int32 sig, const struct sigaction *act, struct sigaction *oldact);
int32 do_sigprocmask(int32 how, const sigset_t *set, sigset_t *oldset);
int32 do_kill(pid_t pid, int32 sig);
int32 do_sigsuspend(const sigset_t *mask);
void do_signal_delivery(void);

/* Signal set operations */
int32 sigemptyset(sigset_t *set);
int32 sigfillset(sigset_t *set);
int32 sigaddset(sigset_t *set, int32 signo);
int32 sigdelset(sigset_t *set, int32 signo);
int32 sigismember(const sigset_t *set, int32 signo);

#endif /* _SIGNAL_H */