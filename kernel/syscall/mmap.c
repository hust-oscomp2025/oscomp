#include <kernel/mmu.h>
#include <kernel/sched.h>
#include <kernel/syscall/syscall.h>
#include <kernel/util.h>
#include <kernel/vfs.h>

int64 sys_mmap(void* addr, size_t length, int32 prot, int32 flags, int32 fd, off_t offset) {
	/* Implementation here */
	return do_mmap(addr, length, prot, flags, fd, offset);
}

int64 do_mmap(void* addr, size_t length, int32 prot, int32 flags, int32 fd, off_t offset) {
	struct mm_struct* mm = current_task()->mm;
	


	/* Implementation here */
	return do_mmap(addr, length, prot, flags, fd, offset);
}




/**
 * Create a new memory mapping
 *
 * @param mm Memory descriptor
 * @param addr Suggested virtual address (0 for auto-allocation)
 * @param length Length of mapping in bytes
 * @param prot Protection flags (PROT_READ/WRITE/EXEC)
 * @param flags Mapping flags (MAP_PRIVATE/SHARED/FIXED/etc)
 * @param file Optional file for file-backed mapping (NULL for anonymous)
 * @param pgoff File offset in pages or physical address for direct mapping
 * @return Mapped virtual address or negative error code
 */
uint64 file_mmap(struct mm_struct* mm, uint64 addr, size_t length, int32 prot, uint64 flags, struct file* file, uint64 pgoff) {
	if (!mm || length == 0) return -EINVAL;

	// Round length to page boundary
	length = ROUNDUP(length, PAGE_SIZE);

	// Determine VMA type based on flags and file
	enum vma_type type;
	if (file)
		type = VMA_FILE;
	else if (flags & MAP_ANONYMOUS)
		type = VMA_ANONYMOUS;
	else if (prot & PROT_EXEC)
		type = VMA_TEXT;
	else
		type = VMA_STACK; // Special case for stack

	// Convert protection flags to vm_flags
	uint64 vm_flags = 0;
	if (prot & PROT_READ) vm_flags |= VM_READ | VM_MAYREAD;
	if (prot & PROT_WRITE) vm_flags |= VM_WRITE | VM_MAYWRITE;
	if (prot & PROT_EXEC) vm_flags |= VM_EXEC | VM_MAYEXEC;

	// Apply mapping flags
	if (flags & MAP_SHARED) vm_flags |= VM_SHARED;
	if (flags & MAP_PRIVATE) vm_flags |= VM_PRIVATE;
	if (flags & MAP_GROWSDOWN) vm_flags |= VM_GROWSDOWN;

	// Find suitable address if needed
	if (addr == 0) {
		addr = find_free_area(mm, length);
	} else if (flags & MAP_FIXED) {
		if (find_vma_intersection(mm, addr, addr + length)) return -EINVAL;
	}

	// Create the VMA
	struct vm_area_struct* vma = vm_area_setup(mm, addr, length, type, prot, vm_flags);
	if (!vma) return -ENOMEM;

	// Handle file-backed mapping
	if (file) {
		vma->vm_file = file;
		vma->vm_pgoff = pgoff;
	}

	// Pre-populate pages if requested
	if (flags & MAP_POPULATE) {
		populate_vma(vma, addr, length, prot);
	}

	// Update code/data boundaries if needed
	if (type == VMA_TEXT) {
		if (mm->start_code == 0 || addr < mm->start_code) mm->start_code = addr;
		if (addr + length > mm->end_code) mm->end_code = addr + length;
	} else if (type == VMA_DATA || type == VMA_FILE) {
		if (mm->start_data == 0 || addr < mm->start_data) mm->start_data = addr;
		if (addr + length > mm->end_data) mm->end_data = addr + length;
	}

	// Return mapped address
	return addr;
}