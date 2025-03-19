/*
 * RISC-V Atomic sb_operations header
 * 
 * Implementation for RISC-V architecture using GNU toolchain
 * Self-contained without external dependencies on Linux kernel headers
 */

 #ifndef _ASM_RISCV_ATOMIC_H
 #define _ASM_RISCV_ATOMIC_H
 
 #include <stdint.h>
 #include <stdatomic.h>
 
 /* 
	* Memory barrier definitions 
	*/
 static inline void mb(void)
 {
		 __asm__ __volatile__ ("fence iorw,iorw" ::: "memory");
 }
 
 static inline void rmb(void)
 {
		 __asm__ __volatile__ ("fence ir,ir" ::: "memory");
 }
 
 static inline void wmb(void)
 {
		 __asm__ __volatile__ ("fence ow,ow" ::: "memory");
 }
 
 static inline void smp_mb(void)
 {
		 mb();
 }
 
 static inline void smp_rmb(void)
 {
		 rmb();
 }
 
 static inline void smp_wmb(void)
 {
		 wmb();
 }
 
 /*
	* Memory access helpers for READ/WRITE_ONCE semantics
	*/
 #define READ_ONCE(x) (*(const volatile typeof(x) *)&(x))
 #define WRITE_ONCE(x, val) (*(volatile typeof(x) *)&(x) = (val))
 
 /*
	* Atomic type definitions
	*/
 typedef struct {
		 volatile int counter;
 } atomic_t;
 
 typedef struct {
		 volatile long counter;
 } atomic64_t;
 
 #define ATOMIC_INIT(i)  { (i) }
 #define ATOMIC64_INIT(i) { (i) }
 
 /**
	* atomic_read - read atomic variable
	* @v: pointer of type atomic_t
	*
	* Atomically reads the value of @v.
	*/
 static inline int atomic_read(const atomic_t *v)
 {
		 return READ_ONCE(v->counter);
 }
 
 /**
	* atomic64_read - read atomic64 variable
	* @v: pointer of type atomic64_t
	*
	* Atomically reads the value of @v.
	*/
 static inline long atomic64_read(const atomic64_t *v)
 {
		 return READ_ONCE(v->counter);
 }
 
 /**
	* atomic_set - set atomic variable
	* @v: pointer of type atomic_t
	* @i: required value
	*
	* Atomically sets the value of @v to @i.
	*/
 static inline void atomic_set(atomic_t *v, int i)
 {
		 WRITE_ONCE(v->counter, i);
 }
 
 /**
	* atomic64_set - set atomic64 variable
	* @v: pointer of type atomic64_t
	* @i: required value
	*
	* Atomically sets the value of @v to @i.
	*/
 static inline void atomic64_set(atomic64_t *v, long i)
 {
		 WRITE_ONCE(v->counter, i);
 }
 
 /**
	* atomic_add - add integer to atomic variable
	* @i: integer value to add
	* @v: pointer of type atomic_t
	*
	* Atomically adds @i to @v.
	*/
 static inline void atomic_add(int i, atomic_t *v)
 {
		 __asm__ __volatile__ (
				 "amoadd.w zero, %1, %0"
				 : "+A" (v->counter)
				 : "r" (i)
				 : "memory");
 }
 
 /**
	* atomic64_add - add integer to atomic64 variable
	* @i: integer value to add
	* @v: pointer of type atomic64_t
	*
	* Atomically adds @i to @v.
	*/
 static inline void atomic64_add(long i, atomic64_t *v)
 {
		 __asm__ __volatile__ (
				 "amoadd.d zero, %1, %0"
				 : "+A" (v->counter)
				 : "r" (i)
				 : "memory");
 }
 
 /**
	* atomic_sub - subtract integer from atomic variable
	* @i: integer value to subtract
	* @v: pointer of type atomic_t
	*
	* Atomically subtracts @i from @v.
	*/
 static inline void atomic_sub(int i, atomic_t *v)
 {
		 atomic_add(-i, v);
 }
 
 /**
	* atomic64_sub - subtract integer from atomic64 variable
	* @i: integer value to subtract
	* @v: pointer of type atomic64_t
	*
	* Atomically subtracts @i from @v.
	*/
 static inline void atomic64_sub(long i, atomic64_t *v)
 {
		 atomic64_add(-i, v);
 }
 
 /**
	* atomic_inc - increment atomic variable
	* @v: pointer of type atomic_t
	*
	* Atomically increments @v by 1.
	*/
 static inline void atomic_inc(atomic_t *v)
 {
		 atomic_add(1, v);
 }
 
 /**
	* atomic64_inc - increment atomic64 variable
	* @v: pointer of type atomic64_t
	*
	* Atomically increments @v by 1.
	*/
 static inline void atomic64_inc(atomic64_t *v)
 {
		 atomic64_add(1, v);
 }
 
 /**
	* atomic_dec - decrement atomic variable
	* @v: pointer of type atomic_t
	*
	* Atomically decrements @v by 1.
	*/
 static inline void atomic_dec(atomic_t *v)
 {
		 atomic_sub(1, v);
 }
 
 /**
	* atomic64_dec - decrement atomic64 variable
	* @v: pointer of type atomic64_t
	*
	* Atomically decrements @v by 1.
	*/
 static inline void atomic64_dec(atomic64_t *v)
 {
		 atomic64_sub(1, v);
 }
 
 /**
	* atomic_add_return - add integer and return
	* @i: integer value to add
	* @v: pointer of type atomic_t
	*
	* Atomically adds @i to @v and returns @i + @v
	*/
 static inline int atomic_add_return(int i, atomic_t *v)
 {
		 int val;
		 
		 __asm__ __volatile__ (
				 "amoadd.w.aqrl %0, %2, %1"
				 : "=r" (val), "+A" (v->counter)
				 : "r" (i)
				 : "memory");
				 
		 return val + i;
 }
 
 /**
	* atomic64_add_return - add integer and return
	* @i: integer value to add
	* @v: pointer of type atomic64_t
	*
	* Atomically adds @i to @v and returns @i + @v
	*/
 static inline long atomic64_add_return(long i, atomic64_t *v)
 {
		 long val;
		 
		 __asm__ __volatile__ (
				 "amoadd.d.aqrl %0, %2, %1"
				 : "=r" (val), "+A" (v->counter)
				 : "r" (i)
				 : "memory");
				 
		 return val + i;
 }
 
 /**
	* atomic_sub_return - subtract integer and return
	* @i: integer value to subtract
	* @v: pointer of type atomic_t
	*
	* Atomically subtracts @i from @v and returns @v - @i
	*/
 static inline int atomic_sub_return(int i, atomic_t *v)
 {
		 return atomic_add_return(-i, v);
 }
 
 /**
	* atomic64_sub_return - subtract integer and return
	* @i: integer value to subtract
	* @v: pointer of type atomic64_t
	*
	* Atomically subtracts @i from @v and returns @v - @i
	*/
 static inline long atomic64_sub_return(long i, atomic64_t *v)
 {
		 return atomic64_add_return(-i, v);
 }
 
 /**
	* atomic_inc_return - increment and return
	* @v: pointer of type atomic_t
	*
	* Atomically increments @v by 1 and returns the result.
	*/
 static inline int atomic_inc_return(atomic_t *v)
 {
		 return atomic_add_return(1, v);
 }
 
 /**
	* atomic64_inc_return - increment and return
	* @v: pointer of type atomic64_t
	*
	* Atomically increments @v by 1 and returns the result.
	*/
 static inline long atomic64_inc_return(atomic64_t *v)
 {
		 return atomic64_add_return(1, v);
 }
 
 /**
	* atomic_dec_return - decrement and return
	* @v: pointer of type atomic_t
	*
	* Atomically decrements @v by 1 and returns the result.
	*/
 static inline int atomic_dec_return(atomic_t *v)
 {
		 return atomic_sub_return(1, v);
 }
 
 /**
	* atomic64_dec_return - decrement and return
	* @v: pointer of type atomic64_t
	*
	* Atomically decrements @v by 1 and returns the result.
	*/
 static inline long atomic64_dec_return(atomic64_t *v)
 {
		 return atomic64_sub_return(1, v);
 }
 
 /**
	* atomic_inc_and_test - increment and test
	* @v: pointer of type atomic_t
	*
	* Atomically increments @v by 1
	* and returns true if the result is zero, or false for all
	* other cases.
	*/
 static inline int atomic_inc_and_test(atomic_t *v)
 {
		 return atomic_inc_return(v) == 0;
 }
 
 /**
	* atomic64_inc_and_test - increment and test
	* @v: pointer of type atomic64_t
	*
	* Atomically increments @v by 1
	* and returns true if the result is zero, or false for all
	* other cases.
	*/
 static inline int atomic64_inc_and_test(atomic64_t *v)
 {
		 return atomic64_inc_return(v) == 0;
 }
 
 /**
	* atomic_dec_and_test - decrement and test
	* @v: pointer of type atomic_t
	*
	* Atomically decrements @v by 1 and
	* returns true if the result is 0, or false for all other
	* cases.
	*/
 static inline int atomic_dec_and_test(atomic_t *v)
 {
		 return atomic_dec_return(v) == 0;
 }
 
 /**
	* atomic64_dec_and_test - decrement and test
	* @v: pointer of type atomic64_t
	*
	* Atomically decrements @v by 1 and
	* returns true if the result is 0, or false for all other
	* cases.
	*/
 static inline int atomic64_dec_and_test(atomic64_t *v)
 {
		 return atomic64_dec_return(v) == 0;
 }
 
 /**
	* atomic_xchg - atomic exchange
	* @v: pointer of type atomic_t
	* @new: integer value to exchange
	*
	* Atomically exchanges the value of @v with @new and returns the old value.
	*/
 static inline int atomic_xchg(atomic_t *v, int new)
 {
		 int val;
		 
		 __asm__ __volatile__ (
				 "amoswap.w.aqrl %0, %2, %1"
				 : "=r" (val), "+A" (v->counter)
				 : "r" (new)
				 : "memory");
				 
		 return val;
 }
 
 /**
	* atomic64_xchg - atomic exchange
	* @v: pointer of type atomic64_t
	* @new: integer value to exchange
	*
	* Atomically exchanges the value of @v with @new and returns the old value.
	*/
 static inline long atomic64_xchg(atomic64_t *v, long new)
 {
		 long val;
		 
		 __asm__ __volatile__ (
				 "amoswap.d.aqrl %0, %2, %1"
				 : "=r" (val), "+A" (v->counter)
				 : "r" (new)
				 : "memory");
				 
		 return val;
 }
 
 /**
	* atomic_cmpxchg - atomic compare and exchange
	* @v: pointer of type atomic_t
	* @old: expected value
	* @new: new value
	*
	* Atomically compares @v with @old and exchanges it with @new if the
	* comparison is successful. Returns the previous value of @v.
	*/
 static inline int atomic_cmpxchg(atomic_t *v, int old, int new)
 {
		 int val;
		 
		 __asm__ __volatile__ (
				 "0:  lr.w %0, %1\n"
				 "    bne %0, %2, 1f\n"
				 "    sc.w.rl t0, %3, %1\n"
				 "    bnez t0, 0b\n"
				 "1:\n"
				 : "=&r" (val), "+A" (v->counter)
				 : "r" (old), "r" (new)
				 : "t0", "memory");
				 
		 return val;
 }
 
 /**
	* atomic64_cmpxchg - atomic compare and exchange
	* @v: pointer of type atomic64_t
	* @old: expected value
	* @new: new value
	*
	* Atomically compares @v with @old and exchanges it with @new if the
	* comparison is successful. Returns the previous value of @v.
	*/
 static inline long atomic64_cmpxchg(atomic64_t *v, long old, long new)
 {
		 long val;
		 
		 __asm__ __volatile__ (
				 "0:  lr.d %0, %1\n"
				 "    bne %0, %2, 1f\n"
				 "    sc.d.rl t0, %3, %1\n"
				 "    bnez t0, 0b\n"
				 "1:\n"
				 : "=&r" (val), "+A" (v->counter)
				 : "r" (old), "r" (new)
				 : "t0", "memory");
				 
		 return val;
 }
 
 /**
	* atomic_add_unless - add unless the number is already a given value
	* @v: pointer of type atomic_t
	* @a: the amount to add to v...
	* @u: ...unless v is equal to u.
	*
	* Atomically adds @a to @v, so long as @v was not already @u.
	* Returns true if the addition was performed.
	*/
 static inline int atomic_add_unless(atomic_t *v, int a, int u)
 {
		 int c, old;
		 c = atomic_read(v);
		 while (c != u && (old = atomic_cmpxchg(v, c, c + a)) != c)
				 c = old;
		 return c != u;
 }
 
 /**
	* atomic64_add_unless - add unless the number is already a given value
	* @v: pointer of type atomic64_t
	* @a: the amount to add to v...
	* @u: ...unless v is equal to u.
	*
	* Atomically adds @a to @v, so long as @v was not already @u.
	* Returns true if the addition was performed.
	*/
 static inline int atomic64_add_unless(atomic64_t *v, long a, long u)
 {
		 long c, old;
		 c = atomic64_read(v);
		 while (c != u && (old = atomic64_cmpxchg(v, c, c + a)) != c)
				 c = old;
		 return c != u;
 }
 
 #define atomic_inc_not_zero(v)          atomic_add_unless((v), 1, 0)
 #define atomic64_inc_not_zero(v)        atomic64_add_unless((v), 1, 0)
 
 /* Atomic sb_operations to memory barriers */
 #define smp_mb__before_atomic()         mb()
 #define smp_mb__after_atomic()          mb()


 // 读取 `sstatus`
static inline uint64_t read_sstatus() {
	uint64_t value;
	asm volatile("csrr %0, sstatus" : "=r"(value));
	return value;
}

// 写入 `sstatus`
static inline void write_sstatus(uint64_t value) {
	asm volatile("csrw sstatus, %0" :: "r"(value));
}

// 禁用 S-mode 中断（清除 SIE 位）
static inline uint64_t disable_irqsave() {
	uint64_t sstatus = read_sstatus();
	write_sstatus(sstatus & ~0x2); // 清除 SIE (bit 1)
	return sstatus;
}

// 恢复 S-mode 中断
static inline void enable_irqrestore(uint64_t flags) {
	write_sstatus(flags);
}

 
 #endif /* _ASM_RISCV_ATOMIC_H */