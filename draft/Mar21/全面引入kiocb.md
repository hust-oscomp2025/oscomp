You're right to be concerned about compatibility. The ideal approach would be a hybrid design:

1. Keep the existing interface for backward compatibility:
   ```c
   ssize_t (*read)(struct file*, char*, size_t, loff_t*);
   ssize_t (*write)(struct file*, const char*, size_t, loff_t*);
   ```

2. Add the kiocb-based interface for advanced operations:
   ```c
   ssize_t (*read_iter)(struct kiocb*, struct io_vector_iterator*);
   ssize_t (*write_iter)(struct kiocb*, struct io_vector_iterator*);
   ```

This way:
- Legacy filesystems continue working with no changes
- The VFS layer provides adaptation between interfaces
- New filesystems can implement the advanced interface directly

This is actually what Linux does - maintaining both interfaces for compatibility while encouraging the newer design pattern.


什么？你说旧的方法怎么办？当然。我们拿它们当“adapter functions”