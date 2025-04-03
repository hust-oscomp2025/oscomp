/* 
 * Comprehensive list of open flags for file open operations
 * These flags control how files are opened and accessed
 */

/*
 * Access mode flags (mutually exclusive - use only one)
 */
#define O_RDONLY    0x0000  /* Open for reading only */
#define O_WRONLY    0x0001  /* Open for writing only */
#define O_RDWR      0x0002  /* Open for reading and writing */
#define O_ACCMODE   0x0003  /* Mask for file access modes */
#define O_EXEC      0x0004  /* Open for execute only */
#define O_SEARCH    0x0005  /* Open for search only (on directories) */

/*
 * Creation and status flags
 */
#define O_CREAT     0x0100  /* Create file if it doesn't exist */
#define O_EXCL      0x0200  /* Exclusive use flag - fail if file exists with O_CREAT */
#define O_NOCTTY    0x0400  /* Don't assign controlling terminal */
#define O_TRUNC     0x1000  /* Truncate file to zero length */
#define O_APPEND    0x2000  /* Append mode - all writes append to end */
#define O_NONBLOCK  0x4000  /* Non-blocking mode */
#define O_NDELAY    O_NONBLOCK  /* Synonym for O_NONBLOCK */

/*
 * Synchronization flags
 */
#define O_SYNC      0x101000  /* Synchronous writes - wait for physical I/O */
#define O_DSYNC     0x001000  /* Synchronized I/O data integrity completion */
#define O_RSYNC     0x101000  /* Synchronized read operations */

/*
 * Special purpose flags
 */
#define O_DIRECT    0x040000  /* Direct disk access - bypass cache */
#define O_LARGEFILE 0x000000  /* Allow 64 bit file sizes (default in 64-bit) */
#define O_DIRECTORY 0x200000  /* Must be a directory */
#define O_NOFOLLOW  0x400000  /* Don't follow symlinks */
#define O_NOATIME   0x800000  /* Don't update access time */
#define O_CLOEXEC   0x080000  /* Close on exec */
#define O_PATH      0x010000  /* Resolve pathname but don't open file */
#define O_TMPFILE   0x020000  /* Create unnamed temporary file (with O_CREAT) */

/*
 * Linux-specific flags
 */
#define O_ASYNC     0x002000  /* Signal-driven I/O */

/*
 * BSD-specific flags
 */
#define O_SHLOCK    0x000010  /* Atomically obtain shared lock */
#define O_EXLOCK    0x000020  /* Atomically obtain exclusive lock */

/*
 * Custom flags specific to the kernel implementation
 */
#define O_BINARY    0x000000  /* Ignored in our implementation (for Unix/Windows compatibility) */
#define O_TEXT      0x000000  /* Ignored in our implementation (for Unix/Windows compatibility) */
#define O_NOINHERIT 0x000040  /* Prevent file handles from being inherited */
#define O_NOSIGPIPE 0x000080  /* Don't generate SIGPIPE on writes when connection closes */

/*
 * Common combinations of open flags
 */
#define O_READ          O_RDONLY             /* Standard read mode */
#define O_WRITE         O_WRONLY             /* Standard write mode */
#define O_READWRITE     O_RDWR               /* Read/write mode */
#define O_CREATE_WRITE  (O_WRONLY | O_CREAT | O_TRUNC)  /* Create and open for writing */
#define O_CREATE_APPEND (O_WRONLY | O_CREAT | O_APPEND) /* Create and append-only writing */