#ifndef _MM_TYPES_H
#define _MM_TYPES_H



typedef unsigned int vm_fault_t;
enum vm_fault_reason {
	VM_FAULT_OOM            = ( vm_fault_t)0x000001,
	VM_FAULT_SIGBUS         = ( vm_fault_t)0x000002,
	VM_FAULT_MAJOR          = ( vm_fault_t)0x000004,
	VM_FAULT_WRITE          = ( vm_fault_t)0x000008,
	VM_FAULT_HWPOISON       = ( vm_fault_t)0x000010,
	VM_FAULT_HWPOISON_LARGE = ( vm_fault_t)0x000020,
	VM_FAULT_SIGSEGV        = ( vm_fault_t)0x000040,
	VM_FAULT_NOPAGE         = ( vm_fault_t)0x000100,
	VM_FAULT_LOCKED         = ( vm_fault_t)0x000200,
	VM_FAULT_RETRY          = ( vm_fault_t)0x000400,
	VM_FAULT_FALLBACK       = ( vm_fault_t)0x000800,
	VM_FAULT_DONE_COW       = ( vm_fault_t)0x001000,
	VM_FAULT_NEEDDSYNC      = ( vm_fault_t)0x002000,
	VM_FAULT_COMPLETED      = ( vm_fault_t)0x004000,
	VM_FAULT_HINDEX_MASK    = ( vm_fault_t)0x0f0000,
};


/* Encode hstate index for a hwpoisoned large page */
#define VM_FAULT_SET_HINDEX(x) (( vm_fault_t)((x) << 16))
#define VM_FAULT_GET_HINDEX(x) ((( uint32)(x) >> 16) & 0xf)

#define VM_FAULT_ERROR (VM_FAULT_OOM | VM_FAULT_SIGBUS |	\
			VM_FAULT_SIGSEGV | VM_FAULT_HWPOISON |	\
			VM_FAULT_HWPOISON_LARGE | VM_FAULT_FALLBACK)

#define VM_FAULT_RESULT_TRACE \
	{ VM_FAULT_OOM,                 "OOM" },	\
	{ VM_FAULT_SIGBUS,              "SIGBUS" },	\
	{ VM_FAULT_MAJOR,               "MAJOR" },	\
	{ VM_FAULT_WRITE,               "WRITE" },	\
	{ VM_FAULT_HWPOISON,            "HWPOISON" },	\
	{ VM_FAULT_HWPOISON_LARGE,      "HWPOISON_LARGE" },	\
	{ VM_FAULT_SIGSEGV,             "SIGSEGV" },	\
	{ VM_FAULT_NOPAGE,              "NOPAGE" },	\
	{ VM_FAULT_LOCKED,              "LOCKED" },	\
	{ VM_FAULT_RETRY,               "RETRY" },	\
	{ VM_FAULT_FALLBACK,            "FALLBACK" },	\
	{ VM_FAULT_DONE_COW,            "DONE_COW" },	\
	{ VM_FAULT_NEEDDSYNC,           "NEEDDSYNC" }



#endif