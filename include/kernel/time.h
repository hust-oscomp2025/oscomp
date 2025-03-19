#ifndef _KERNEL_TIME_H
#define _KERNEL_TIME_H

#include <kernel/types.h>
#include <time.h>

/* Time limits and constants */
#define TIME_T_MAX      ((1UL << ((sizeof(time_t) << 3) - 1)) - 1)
#define NSEC_PER_SEC    1000000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_USEC   1000L

/* Clock identifiers */
#define CLOCK_REALTIME          0   /* System-wide real-time clock */
#define CLOCK_MONOTONIC         1   /* Monotonic system-wide clock */
#define CLOCK_BOOTTIME          2   /* Monotonic clock that includes time system was suspended */
#define CLOCK_PROCESS_CPUTIME_ID 3  /* Per-process CPU time clock */

/* Time comparison and manipulation functions */

/*
 * Compare two timespec values
 * Returns:  1 if a > b
 *           0 if a == b
 *          -1 if a < b
 */
static inline int timespec_compare(const struct timespec *a, const struct timespec *b)
{
    if (a->tv_sec != b->tv_sec)
        return a->tv_sec > b->tv_sec ? 1 : -1;
    return a->tv_nsec > b->tv_nsec ? 1 : (a->tv_nsec < b->tv_nsec ? -1 : 0);
}

/* 
 * Add two timespec values: result = a + b
 */
static inline void timespec_add(struct timespec *result, const struct timespec *a, const struct timespec *b)
{
    result->tv_sec = a->tv_sec + b->tv_sec;
    result->tv_nsec = a->tv_nsec + b->tv_nsec;
    if (result->tv_nsec >= NSEC_PER_SEC) {
        result->tv_sec++;
        result->tv_nsec -= NSEC_PER_SEC;
    }
}

/* 
 * Subtract two timespec values: result = a - b
 */
static inline void timespec_sub(struct timespec *result, const struct timespec *a, const struct timespec *b)
{
    result->tv_sec = a->tv_sec - b->tv_sec;
    if (a->tv_nsec >= b->tv_nsec) {
        result->tv_nsec = a->tv_nsec - b->tv_nsec;
    } else {
        result->tv_sec--;
        result->tv_nsec = NSEC_PER_SEC + a->tv_nsec - b->tv_nsec;
    }
}

/* Convert between time structures */

/* 
 * Convert timespec to timeval
 */
static inline void timespec_to_timeval(struct timeval *tv, const struct timespec *ts)
{
    tv->tv_sec = ts->tv_sec;
    tv->tv_usec = ts->tv_nsec / NSEC_PER_USEC;
}

/* 
 * Convert timeval to timespec
 */
static inline void timeval_to_timespec(struct timespec *ts, const struct timeval *tv)
{
    ts->tv_sec = tv->tv_sec;
    ts->tv_nsec = tv->tv_usec * NSEC_PER_USEC;
}

/* Core time function prototypes */

/*
 * Get current time (CLOCK_REALTIME)
 */
int do_gettimeofday(struct timeval *tv);

/*
 * Set system time
 */
int do_settimeofday(const struct timeval *tv);

/*
 * Get time from specified clock
 */
int do_clock_gettime(clockid_t which_clock, struct timespec *tp);

/*
 * Set time of specified clock
 */
int do_clock_settime(clockid_t which_clock, const struct timespec *tp);

/*
 * Get resolution of specified clock
 */
int do_clock_getres(clockid_t which_clock, struct timespec *tp);

/*
 * Suspend execution for time interval
 */
int do_nanosleep(const struct timespec *req, struct timespec *rem);

/* Timer functions */

/*
 * Create a timer
 */
int timer_create(clockid_t clockid, struct sigevent *evp, timer_t *timerid);

/*
 * Delete a timer
 */
int timer_delete(timer_t timerid);

/*
 * Arm/disarm a timer
 */
int timer_settime(timer_t timerid, int flags, 
                 const struct itimerspec *value, struct itimerspec *ovalue);

/*
 * Get remaining time on a timer
 */
int timer_gettime(timer_t timerid, struct itimerspec *value);

/* Internal kernel time management */

/*
 * Convert jiffies to timespec
 */
void jiffies_to_timespec(unsigned long jiffies, struct timespec *value);

/*
 * Update system time from hardware clock
 */
void update_sys_time_from_hw(void);

/*
 * Initialize time subsystem
 */
void time_init(void);

#endif /* _KERNEL_TIME_H */