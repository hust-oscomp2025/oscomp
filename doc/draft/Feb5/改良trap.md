/**
 * 处理内核模式下的陷阱
 * 当在内核模式下发生异常或中断时被调用
 */
void kernel_trap_handler(void) {
  uint64 cause = read_csr(scause);
  uint64 epc = read_csr(sepc);
  uint64 stval = read_csr(stval);
  
  // 检查是否是中断（最高位为1表示中断）
  if (cause & (1ULL << 63)) {
    uint64 interrupt_cause = cause & ~(1ULL << 63); // 去掉最高位获取中断类型
    
    // 处理不同类型的中断
    switch (interrupt_cause) {
      case IRQ_S_TIMER:
        kprintf("内核中断: IRQ_S_TIMER (S模式计时器中断)\n");
        handle_mtimer_trap();
        break;
      case IRQ_S_SOFT:
        kprintf("内核中断: IRQ_S_SOFT (S模式软件中断)\n");
        // 处理软件中断
        break;
      case IRQ_S_EXT:
        kprintf("内核中断: IRQ_S_EXT (S模式外部中断)\n");
        // 处理外部中断
        break;
      default:
        kprintf("内核中断: 未知类型 (代码: %p)\n", interrupt_cause);
        break;
    }
  } else {
    // 处理异常（非中断）
    switch (cause) {
      case CAUSE_MISALIGNED_FETCH:
        kprintf("内核异常: CAUSE_MISALIGNED_FETCH (取指未对齐)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 取指未对齐");
        break;
      case CAUSE_FETCH_ACCESS:
        kprintf("内核异常: CAUSE_FETCH_ACCESS (取指访问错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 取指访问错误");
        break;
      case CAUSE_ILLEGAL_INSTRUCTION:
        kprintf("内核异常: CAUSE_ILLEGAL_INSTRUCTION (非法指令)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 非法指令");
        break;
      case CAUSE_BREAKPOINT:
        kprintf("内核异常: CAUSE_BREAKPOINT (断点)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        // 可以选择不panic，而是处理断点
        break;
      case CAUSE_MISALIGNED_LOAD:
        kprintf("内核异常: CAUSE_MISALIGNED_LOAD (加载未对齐)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 加载未对齐");
        break;
      case CAUSE_LOAD_ACCESS:
        kprintf("内核异常: CAUSE_LOAD_ACCESS (加载访问错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 加载访问错误");
        break;
      case CAUSE_MISALIGNED_STORE:
        kprintf("内核异常: CAUSE_MISALIGNED_STORE (存储未对齐)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 存储未对齐");
        break;
      case CAUSE_STORE_ACCESS:
        kprintf("内核异常: CAUSE_STORE_ACCESS (存储访问错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 存储访问错误");
        break;
      case CAUSE_USER_ECALL:
        kprintf("内核异常: CAUSE_USER_ECALL (用户态系统调用)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        // 这通常不应该在内核模式下发生
        panic("内核异常: 不应该在内核模式收到用户态系统调用");
        break;
      case CAUSE_SUPERVISOR_ECALL:
        kprintf("内核异常: CAUSE_SUPERVISOR_ECALL (内核态系统调用)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        // 处理内核态系统调用
        break;
      case CAUSE_MACHINE_ECALL:
        kprintf("内核异常: CAUSE_MACHINE_ECALL (机器态系统调用)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 机器态系统调用");
        break;
      case CAUSE_FETCH_PAGE_FAULT:
        kprintf("内核异常: CAUSE_FETCH_PAGE_FAULT (取指页错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 取指页错误");
        break;
      case CAUSE_LOAD_PAGE_FAULT:
        kprintf("内核异常: CAUSE_LOAD_PAGE_FAULT (加载页错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 加载页错误");
        break;
      case CAUSE_STORE_PAGE_FAULT:
        kprintf("内核异常: CAUSE_STORE_PAGE_FAULT (存储页错误)\n");
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 存储页错误");
        break;
      default:
        kprintf("内核异常: 未知类型 (代码: %p)\n", cause);
        kprintf("  epc = %p, stval = %p\n", epc, stval);
        panic("内核异常: 未知类型");
        break;
    }
  }

  // 恢复中断
  write_csr(sstatus, read_csr(sstatus) | SSTATUS_SIE);
}

/**
 * 处理用户空间页错误
 * 包含了原有的栈自动增长和COW功能，以及新的VMA管理功能
 * 
 * @param mcause 错误原因
 * @param sepc 错误发生时的程序计数器
 * @param stval 访问时导致页错误的虚拟地址
 */
void handle_user_page_fault(uint64 mcause, uint64 sepc, uint64 stval) {
  struct task_struct *proc = CURRENT;
  uint64 addr = stval;
  
  kprintf("sepc=%lx, handle_page_fault: %lx\n", sepc, addr);
  
  // 标记访问类型
  int32 fault_prot = 0;
  if (mcause == CAUSE_LOAD_PAGE_FAULT)
    fault_prot |= PROT_READ;
  else if (mcause == CAUSE_STORE_PAGE_FAULT)
    fault_prot |= PROT_WRITE;
  else if (mcause == CAUSE_FETCH_PAGE_FAULT)
    fault_prot |= PROT_EXEC;
  
  // 处理基于 VMA 的内存管理
  if (proc->mm) {
    // 查找地址所在的VMA
    struct vm_area_struct *vma = find_vma(proc->mm, addr);
    
    if (vma) {
      // 确认访问权限
      if ((fault_prot & vma->vm_prot) != fault_prot) {
        kprintf("权限不足: 需要 %d, VMA允许 %d\n", fault_prot, vma->vm_prot);
        goto error;
      }
      
      // 页地址对齐
      uint64 page_va = ROUNDDOWN(addr, PAGE_SIZE);
      
      // 计算页索引
      int32 page_idx = (page_va - vma->vm_start) / PAGE_SIZE;
      if (page_idx < 0 || page_idx >= vma->page_count) {
        kprintf("页索引越界: %d\n", page_idx);
        goto error;
      }
      
      // 如果页已存在，检查是否需要COW
      if (vma->pages[page_idx]) {
        // 检查是否是COW页
        pte_t *pte = page_walk(proc->pagetable, page_va, 0);
        if (pte && (*pte & PTE_V) && ((*pte & PTE_W) == 0) && (*pte & PTE_R) && mcause == CAUSE_STORE_PAGE_FAULT) {
          // 写时复制
          struct page *old_page = vma->pages[page_idx];
          struct page *new_page = alloc_page();
          if (!new_page) goto error;
          
          // 复制页内容
          uint64 old_pa = page_to_addr(old_page);
          uint64 new_pa = page_to_addr(new_page);
          memcpy((void*)new_pa, (void*)old_pa, PAGE_SIZE);
          
          // 更新映射
          user_vm_unmap(proc->pagetable, page_va, PAGE_SIZE, 0);
          user_vm_map(proc->pagetable, page_va, PAGE_SIZE, new_pa, 
                  prot_to_type(vma->vm_prot, 1));
          
          // 更新页结构
          vma->pages[page_idx] = new_page;
          new_page->paddr = page_va;
          put_page(old_page);
          
          return;
        } else {
          // 页已分配但可能未映射，确保映射正确
          uint64 pa = page_to_addr(vma->pages[page_idx]);
          user_vm_map(proc->pagetable, page_va, PAGE_SIZE, pa,
                  prot_to_type(vma->vm_prot, 1));
          return;
        }
      }
      
      // 分配新页并映射
      struct page *new_page = alloc_page();
      if (!new_page) goto error;
      
      uint64 pa = page_to_addr(new_page);
      // 清零新页
      memset((void*)pa, 0, PAGE_SIZE);
      
      // 更新映射
      user_vm_map(proc->pagetable, page_va, PAGE_SIZE, pa,
              prot_to_type(vma->vm_prot, 1));
      
      // 更新VMA页表
      vma->pages[page_idx] = new_page;
      new_page->paddr = page_va;
      
      return;
    }
  }
  
  // 如果没有找到VMA或VMA处理失败，回退到原有处理逻辑
  if (mcause == CAUSE_STORE_PAGE_FAULT) {
    kprintf("用户页错误: CAUSE_STORE_PAGE_FAULT (存储页错误)\n");
    kprintf("  处理地址: %lx\n", addr);
    
    // 检查是否是栈扩展
    if (addr >= proc->user_stack_bottom - PAGE_SIZE) {
      kprintf("  检测到栈扩展请求\n");
      proc->user_stack_bottom -= PAGE_SIZE;
      uint64 pa = (uint64)Alloc_page();
      kprintf("  分配新页面: 物理地址 = %lx\n", pa);
      user_vm_map(proc->pagetable, proc->user_stack_bottom, PAGE_SIZE,
              pa, prot_to_type(PROT_WRITE | PROT_READ, 1));
      proc->mapped_info[STACK_SEGMENT].va = proc->user_stack_bottom;
      proc->mapped_info[STACK_SEGMENT].npages++;
      kprintf("  栈扩展成功: 新栈底 = %lx\n", proc->user_stack_bottom);
      return;
    }
    
    // 检查是否是COW页
    pte_t *pte = page_walk(proc->pagetable, ROUNDDOWN(addr, PAGE_SIZE), 0);
    if (pte && (*pte & PTE_V) && ((*pte & PTE_W) == 0) && (*pte & PTE_R)) {
      kprintf("  检测到COW页面\n");
      uint64 page_va = ROUNDDOWN(addr, PAGE_SIZE);
      uint64 newpa = (uint64)Alloc_page();
      kprintf("  COW: 复制页面内容从 %lx 到 %lx\n", PTE2PA(*pte), newpa);
      memcpy((void*)newpa, (void*)PTE2PA(*pte), PAGE_SIZE);
      user_vm_unmap(proc->pagetable, page_va, PAGE_SIZE, 0);
      user_vm_map(proc->pagetable, page_va, PAGE_SIZE, newpa, 
              prot_to_type(PROT_WRITE | PROT_READ, 1));
      kprintf("  COW处理成功: 虚拟地址 = %lx\n", page_va);
      return;
    }
  } else if (mcause == CAUSE_LOAD_PAGE_FAULT) {
    kprintf("用户页错误: CAUSE_LOAD_PAGE_FAULT (加载页错误)\n");
    kprintf("  处理地址: %lx\n", addr);
  } else if (mcause == CAUSE_FETCH_PAGE_FAULT) {
    kprintf("用户页错误: CAUSE_FETCH_PAGE_FAULT (取指页错误)\n");
    kprintf("  处理地址: %lx\n", addr);
  }
  
error:
  // 不能处理的页错误
  kprintf("无法处理的页错误: addr=%lx, mcause=%lx\n", stval, mcause);
  panic("This address is not available!");
}