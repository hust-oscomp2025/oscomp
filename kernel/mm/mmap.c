

#include <kernel/mm/page.h>
#include <kernel/mm/kmalloc.h>
#include <kernel/mm/mmap.h>
#include <kernel/mm/mm_struct.h>
#include <kernel/mm/pagetable.h>

#include <kernel/process.h>
#include <util/string.h>
#include <errno.h>

/**
 * 将保护标志(PROT_*)转换为页表项标志
 */
uint64 prot_to_type(int prot, int user) {
  uint64 perm = 0;
  if (prot & PROT_READ)
    perm |= PTE_R | PTE_A;
  if (prot & PROT_WRITE)
    perm |= PTE_W | PTE_D;
  if (prot & PROT_EXEC)
    perm |= PTE_X | PTE_A;
  if (perm == 0)
    perm = PTE_R;
  if (user)
    perm |= PTE_U;
  return perm;
}

/**
 * 进程内存映射 - 将物理页映射到进程的虚拟地址空间
 */
int proc_map_page(struct task_struct *proc, uaddr vaddr, struct page *page, int prot) {
    // 参数检查
    if (!proc || !proc->mm || !proc->mm->pagetable)
        return -EINVAL;

    // 转换保护标志为页表项标志
    uint64 perm = prot_to_type(prot, 1); // 1表示用户页

    // 获取物理页地址
    uint64 pa = (uint64)page_to_virt(page);

    // 使用页表管理接口建立映射
    int result = pgt_map_page(proc->mm->pagetable, vaddr, pa, perm);
    
    // 映射成功后，更新VMA信息（此处简化处理）
    if (result == 0) {
        // 在实际实现中，可能需要更新或创建VMA
        // 这里简化处理，仅刷新TLB
        flush_tlb();
    }

    return result;
}

/**
 * 取消进程的内存映射
 */
int proc_unmap_page(struct task_struct *proc, uaddr vaddr) {
    // 参数检查
    if (!proc || !proc->mm)
        return -EINVAL;

    // 确保地址页对齐
    if (vaddr & (PAGE_SIZE - 1))
        return -EINVAL;

    // 获取进程页表
    pagetable_t pagetable = proc->mm->pagetable;
    if (!pagetable)
        return -EINVAL;

    // 使用页表管理接口取消映射
    int result = pgt_unmap(pagetable, vaddr, PAGE_SIZE, 0); // 不释放物理页
    
    // 取消映射成功后，更新VMA信息（此处简化处理）
    if (result == 0) {
        // 在实际实现中，可能需要更新VMA
        // 这里简化处理，仅刷新TLB
        flush_tlb();
    }

    return result;
}

/**
 * 修改进程内存映射的保护属性
 */
int proc_protect_page(struct task_struct *proc, uaddr vaddr, int prot) {
    // 参数检查
    if (!proc || !proc->mm)
        return -EINVAL;
    
    // 确保地址页对齐
    if (vaddr & (PAGE_SIZE - 1))
        return -EINVAL;
    
    // 获取进程页表
    pagetable_t pagetable = proc->mm->pagetable;
    if (!pagetable)
        return -EINVAL;

    // 查找对应的页表项
    pte_t *pte = pgt_walk(pagetable, vaddr, 0);
    if (!pte || !(*pte & PTE_V))
        return -EFAULT; // 映射不存在
    
    // 转换保护标志为页表项标志
    uint64 perm = prot_to_type(prot, 1); // 1表示用户页
    
    // 保留物理页地址，更新权限位
    uint64 pa = PTE2PA(*pte);
    *pte = PA2PPN(pa) | perm;
    
    // 刷新TLB
    flush_tlb();
    
    return 0;
}

/**
 * 查询进程内存映射状态
 */
int proc_query_mapping(struct task_struct *proc, uaddr addr, 
                     struct page **page_out, int *prot_out) {
    // 参数检查
    if (!proc || !proc->mm)
        return -EINVAL;
    
    // 获取进程页表
    pagetable_t pagetable = proc->mm->pagetable;
    if (!pagetable)
        return -EINVAL;

    // 查找对应的页表项
    pte_t *pte = pgt_walk(pagetable, addr, 0);
    if (!pte || !(*pte & PTE_V))
        return -EFAULT; // 映射不存在

    // 获取物理地址
    uint64 pa = PTE2PA(*pte);
    
    // 如果请求返回物理页结构
    if (page_out) {
        *page_out = virt_to_page((void*)pa);
    }
    
    // 如果请求返回保护标志
    if (prot_out) {
        int prot = 0;
        if (*pte & PTE_R)
            prot |= PROT_READ;
        if (*pte & PTE_W)
            prot |= PROT_WRITE;
        if (*pte & PTE_X)
            prot |= PROT_EXEC;
        *prot_out = prot;
    }

    return 0;
}

int proc_alloc_map_page(struct task_struct *proc, uaddr vaddr, int prot) {
    // 参数检查
    if (!proc || !proc->mm || !proc->mm->pagetable|| (vaddr & (PAGE_SIZE - 1))  )
        return -EINVAL;

    // 检查地址是否已经映射
    pte_t *pte = pgt_walk(proc->mm->pagetable, vaddr, 0);
    if (pte && (*pte & PTE_V))
        return -EEXIST; // 已存在映射

    // 分配物理页
    struct page *page = alloc_page();
    if (!page)
        return -ENOMEM;

    // 获取页的虚拟地址并清零
    void *page_va = page_to_virt(page);
    memset(page_va, 0, PAGE_SIZE);

    // 映射页到进程地址空间
    int result = proc_map_page(proc, vaddr, page, prot);
    if (result != 0) {
        // 映射失败，释放页
        free_page(page);
        return result;
    }

    return 0;
}






/**
 * 取消进程内存映射并释放对应的物理页
 */
int proc_unmap_free_page(struct task_struct *proc, uaddr vaddr) {
    // 参数检查
    if (!proc || !proc->mm)
        return -EINVAL;

    // 确保地址页对齐
    if (vaddr & (PAGE_SIZE - 1))
        return -EINVAL;

    // 查询当前映射
    struct page *page = NULL;
    int result = proc_query_mapping(proc, vaddr, &page, NULL);
    if (result != 0)
        return result; // 映射不存在

    // 取消映射
    result = proc_unmap_page(proc, vaddr);
    if (result != 0)
        return result;

    // 释放物理页
    if (page)
        free_page(page);
    
    return 0;
}