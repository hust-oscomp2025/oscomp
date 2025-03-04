#ifndef _ELF_H_
#define _ELF_H_


#include "util/types.h"
#include "process.h"

#define MAX_CMDLINE_ARGS 64

// elf header structure
typedef struct elf_header_t {
  uint32 magic;
  uint8 elf[12];
  uint16 type;      /* Object file type */
  uint16 machine;   /* Architecture */
  uint32 version;   /* Object file version */
  uint64 entry;     /* Entry point virtual address */
  uint64 phoff;     /* Program header table file offset */
  uint64 shoff;     /* Section header table file offset */
  uint32 flags;     /* Processor-specific flags */
  uint16 ehsize;    /* ELF header size in bytes */
  uint16 phentsize; /* Program header table entry size */
  uint16 phnum;     /* Program header table entry count */
  uint16 shentsize; /* Section header table entry size */
  uint16 shnum;     /* Section header table entry count */
  uint16 shstrndx;  /* Section header string table index */
} elf_header;

// segment types, attributes of elf_prog_header_t.flags
#define SEGMENT_READABLE   0x4
#define SEGMENT_EXECUTABLE 0x1
#define SEGMENT_WRITABLE   0x2

// Program segment header.
typedef struct elf_prog_header_t {
  uint32 type;   /* Segment type */
  uint32 flags;  /* Segment flags */
  uint64 off;    /* Segment file offset */
  uint64 vaddr;  /* Segment virtual address */
  uint64 paddr;  /* Segment physical address */
  uint64 filesz; /* Segment size in file */
  uint64 memsz;  /* Segment size in memory */
  uint64 align;  /* Segment alignment */
} elf_prog_header;
// compilation units header (in debug line section)
typedef struct __attribute__((packed)) {
    uint32 length;
    uint16 version;
    uint32 header_length;
    uint8 min_instruction_length;
    uint8 default_is_stmt;
    int8 line_base;
    uint8 line_range;
    uint8 opcode_base;
    uint8 std_opcode_lengths[12];
} debug_header;
#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian
#define ELF_PROG_LOAD 1

typedef enum elf_status_t {
  EL_OK = 0,

  EL_EIO,
  EL_ENOMEM,
  EL_NOTELF,
  EL_ERR,

} elf_status;

typedef struct elf_ctx_t {
  void *info;
  elf_header ehdr;
} elf_ctx;

elf_status elf_init(elf_ctx *ctx, void *info);
elf_status elf_load(elf_ctx *ctx);

void load_elf_from_file(process *p, char* filename);

//lab1_challenge1
// ELF符号表结构体
typedef struct elf_symbol_t{
  uint32 name;		/* Symbol name, index in string tbl */
  unsigned char	info;	/* Type and binding attributes */
  unsigned char	other;	/* No defined meaning, 0 */
  uint16 shndx;		/* Associated section index */
  uint64 value;		/* Value of the symbol */
  uint64 size;		/* Associated symbol size */
} elf_symbol;

// ELF节头表结构体
typedef struct elf_section_header_t {
  uint32 name;        /* 节的名称（该名称是一个指向节头字符串表的索引，字符串表包含节名） */
  uint32 type;        /* 节的类型（例如，SHT_PROGBITS表示程序数据段，SHT_SYMTAB表示符号表） */
  uint64 flags;       /* 节的标志位（例如，SHF_ALLOC表示该节会被加载到内存） */
  uint64 addr;        /* 节的虚拟地址（该节在内存中的起始位置） */
  uint64 offset;      /* 节在文件中的偏移量（该节在ELF文件中的位置） */
  uint64 size;        /* 节的大小（该节在文件中的字节数） */
  uint32 link;        /* 链接字段（对于某些类型的节，如符号表，它指向其他节的索引） */
  uint32 info;        /* 附加信息（具体含义取决于节的类型） */
  uint64 addralign;   /* 节的对齐要求（通常是2的幂，用于内存对齐） */
  uint64 entsize;     /* 每个条目的大小（如果节包含多个条目，如符号表） */
} elf_sect_header;

// 定义 ELF 节类型
#define ELF_SHT_NULL       0x0    // Null 节，表示节头表中的空节
#define ELF_SHT_PROGBITS   0x1    // 程序数据段，包含程序运行时的数据
#define ELF_SHT_SYMTAB     0x2    // 符号表，存储符号信息
#define ELF_SHT_STRTAB     0x3    // 字符串表，存储字符串信息
#define ELF_SHT_RELA       0x4    // 重新定位表
#define ELF_SHT_HASH       0x5    // 哈希表，通常用于动态链接
#define ELF_SHT_DYNAMIC    0x6    // 动态段
#define ELF_SHT_NOTE       0x7    // 注释
#define ELF_SHT_NOBITS     0x8    // 不占用内存的节
#define ELF_SHT_REL        0x9    // 重新定位表（另一种类型）
#define ELF_SHT_SHLIB      0x0A   // 保留给系统使用的节
#define ELF_SHT_DYNSYM     0x0B   // 动态符号表

// 定义节标志（通常用于表示节的特性，如是否可读、可写等）
#define ELF_SHF_WRITE      0x1    // 可写
#define ELF_SHF_ALLOC      0x2    // 分配内存
#define ELF_SHF_EXECINSTR  0x4    // 可执行

elf_status load_debug_infomation(elf_ctx *ctx);
void print_elf_symbol(const elf_symbol *symbol, int index);
char* locate_function_name(uint64 epc);

#define SYMBOL_NUM 400
#define SYMBOL_LENGTH 1000
extern elf_symbol funtion_symbols[SYMBOL_NUM];
extern char function_names[SYMBOL_NUM][SYMBOL_LENGTH];
extern int function_count;

// lab1_challenge2
void load_debugline(elf_ctx *ctx);
void print_chars(const char* start, int n);
void print_elf_section_header(elf_sect_header *section_header, char *shstr);



#endif
