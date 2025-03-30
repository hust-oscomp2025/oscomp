For implementing a console system using your character device and buffer infrastructure, here's a comprehensive approach based on Linux's implementation:

### 1. Console Character Device Implementation

```c
/**
 * struct console_dev - Private data for console character device
 */
struct console_dev {
    spinlock_t buffer_lock;       /* Protects circular buffer */
    char buffer[CONSOLE_BUFFER_SIZE];
    int head;                     /* Write position */
    int tail;                     /* Read position */
    struct wait_queue_head read_wait;  /* Wait queue for readers */
};

static struct char_device console_cdev;
static struct console_dev console_data;

/* Console character device operations */
static int console_open(struct char_device* cdev, struct file* file) {
    file->private_data = &console_data;
    return 0;
}

static int console_release(struct char_device* cdev, struct file* file) {
    return 0;
}

static ssize_t console_read(struct char_device* cdev, struct file* file,
                          char* buf, size_t count, loff_t* ppos) {
    struct console_dev* con = &console_data;
    size_t read_count = 0;
    
    spinlock_lock(&con->buffer_lock);
    
    /* Wait until there's data to read */
    while (con->head == con->tail) {
        spinlock_unlock(&con->buffer_lock);
        if (file->f_flags & O_NONBLOCK)
            return -EAGAIN;
        if (wait_event_interruptible(con->read_wait, con->head != con->tail))
            return -EINTR;
        spinlock_lock(&con->buffer_lock);
    }
    
    /* Read data from buffer */
    while (read_count < count && con->head != con->tail) {
        buf[read_count++] = con->buffer[con->tail];
        con->tail = (con->tail + 1) % CONSOLE_BUFFER_SIZE;
    }
    
    spinlock_unlock(&con->buffer_lock);
    return read_count;
}

static ssize_t console_write(struct char_device* cdev, struct file* file,
                           const char* buf, size_t count, loff_t* ppos) {
    /* Direct hardware write for console output */
    for (size_t i = 0; i < count; i++) {
        console_putchar(buf[i]);
    }
    return count;
}

static struct char_device_operations console_ops = {
    .open = console_open,
    .release = console_release,
    .read = console_read,
    .write = console_write
};
```

### 2. Console Buffer Management for `sprint`

```c
/**
 * console_buffer_write - Write data to console buffer
 * @buf: Data to write
 * @count: Number of bytes
 * 
 * Adds data to the circular buffer and wakes up any waiting readers
 */
static void console_buffer_write(const char* buf, size_t count) {
    struct console_dev* con = &console_data;
    size_t i;
    
    if (!buf || count == 0)
        return;
    
    spinlock_lock(&con->buffer_lock);
    
    /* Write data to buffer */
    for (i = 0; i < count; i++) {
        con->buffer[con->head] = buf[i];
        con->head = (con->head + 1) % CONSOLE_BUFFER_SIZE;
        
        /* If buffer full, drop oldest data */
        if (con->head == con->tail)
            con->tail = (con->tail + 1) % CONSOLE_BUFFER_SIZE;
    }
    
    spinlock_unlock(&con->buffer_lock);
    
    /* Wake up readers */
    wake_up_interruptible(&con->read_wait);
}
```

### 3. Hardware-Level Output Function

```c
/**
 * console_putchar - Write a character directly to console hardware
 * @c: Character to write
 * 
 * This is a low-level function that directly outputs to hardware
 * Useful for early boot and kernel panics when normal paths aren't available
 */
static void console_putchar(char c) {
    /* Example for RISC-V QEMU UART at 0x10000000 */
    volatile uint8_t* uart = (volatile uint8_t*)0x10000000;
    
    /* Special handling for newline */
    if (c == '\n')
        console_putchar('\r');
        
    /* Wait until UART is ready to transmit */
    while (!(*(uart + 5) & 0x20))
        ;
        
    /* Send character */
    *uart = c;
}
```

### 4. Implementing `sprint` Function

```c
/**
 * sprint - Format and output a message to the console
 * @fmt: Format string
 * @...: Arguments for formatting
 *
 * This is the main kernel logging function, similar to Linux's printk
 * Outputs to both the console buffer and directly to hardware
 */
int sprint(const char* fmt, ...) {
    char buf[256];
    va_list args;
    int printed;
    
    /* Format the message */
    va_start(args, fmt);
    printed = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (printed > 0) {
        /* Add to console buffer for /dev/console readers */
        console_buffer_write(buf, printed);
        
        /* Also write directly to hardware */
        for (int i = 0; i < printed; i++) {
            console_putchar(buf[i]);
        }
    }
    
    return printed;
}
```

### 5. Console Initialization

```c
/**
 * console_init - Initialize console subsystem
 *
 * Sets up the console character device and internal data structures
 */
int console_init(void) {
    struct console_dev* con = &console_data;
    
    /* Initialize buffer */
    spinlock_init(&con->buffer_lock);
    con->head = con->tail = 0;
    init_waitqueue_head(&con->read_wait);
    
    /* Set up character device */
    memset(&console_cdev, 0, sizeof(console_cdev));
    console_cdev.cd_dev = MKDEV(5, 1); /* Standard console major/minor */
    console_cdev.ops = &console_ops;
    atomic_set(&console_cdev.cd_count, 1);
    
    /* Register the character device */
    /* In a complete implementation, you'd register with your device system */
    
    sprint("Console initialized\n");
    return 0;
}
```

This implementation follows Linux's approach with some simplifications for your system. The key aspects are:

1. A circular buffer for console messages
2. Direct hardware output for immediate display
3. A character device interface for userspace access
4. Proper synchronization using spinlocks and wait queues

This provides both early boot console output and a standard device file interface.