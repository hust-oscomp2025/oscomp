/**
 * @file ext2.h
 * @brief Ext2文件系统实现
 * 
 * 定义了基本的Ext2文件系统结构和操作，用于RISC-V操作系统
 */

 #ifndef _KERNEL_EXT2_H_
 #define _KERNEL_EXT2_H_
 
 #include <kstdlib.h>
 #include <kernel.h>
 #include <riscv.h>

 
 /* Ext2文件系统魔数 */
 #define EXT2_MAGIC      0xEF53
 
 /* Ext2文件系统状态 */
 #define EXT2_VALID_FS   0x0001  /* 文件系统干净 */
 #define EXT2_ERROR_FS   0x0002  /* 文件系统有错误 */
 
 /* Ext2错误处理方法 */
 #define EXT2_ERRORS_CONTINUE    1  /* 继续运行 */
 #define EXT2_ERRORS_RO          2  /* 重新挂载为只读 */
 #define EXT2_ERRORS_PANIC       3  /* 内核恐慌 */
 
 /* Ext2创建者操作系统 */
 #define EXT2_OS_LINUX           0  /* Linux */
 #define EXT2_OS_HURD            1  /* GNU HURD */
 #define EXT2_OS_MASIX           2  /* MASIX */
 #define EXT2_OS_FREEBSD         3  /* FreeBSD */
 #define EXT2_OS_LITES           4  /* Lites */
 
 /* Ext2文件类型 */
 #define EXT2_S_IFMT  0xF000     /* 文件类型掩码 */
 #define EXT2_S_IFSOCK 0xC000    /* 套接字 */
 #define EXT2_S_IFLNK  0xA000    /* 符号链接 */
 #define EXT2_S_IFREG  0x8000    /* 普通文件 */
 #define EXT2_S_IFBLK  0x6000    /* 块设备 */
 #define EXT2_S_IFDIR  0x4000    /* 目录 */
 #define EXT2_S_IFCHR  0x2000    /* 字符设备 */
 #define EXT2_S_IFIFO  0x1000    /* FIFO */
 
 /* Ext2文件权限 */
 #define EXT2_S_ISUID  0x0800    /* SUID */
 #define EXT2_S_ISGID  0x0400    /* SGID */
 #define EXT2_S_ISVTX  0x0200    /* Sticky bit */
 #define EXT2_S_IRUSR  0x0100    /* 用户可读 */
 #define EXT2_S_IWUSR  0x0080    /* 用户可写 */
 #define EXT2_S_IXUSR  0x0040    /* 用户可执行 */
 #define EXT2_S_IRGRP  0x0020    /* 组可读 */
 #define EXT2_S_IWGRP  0x0010    /* 组可写 */
 #define EXT2_S_IXGRP  0x0008    /* 组可执行 */
 #define EXT2_S_IROTH  0x0004    /* 其他可读 */
 #define EXT2_S_IWOTH  0x0002    /* 其他可写 */
 #define EXT2_S_IXOTH  0x0001    /* 其他可执行 */
 
 /* Ext2目录项类型 */
 #define EXT2_FT_UNKNOWN   0     /* 未知类型 */
 #define EXT2_FT_REG_FILE  1     /* 普通文件 */
 #define EXT2_FT_DIR       2     /* 目录 */
 #define EXT2_FT_CHRDEV    3     /* 字符设备 */
 #define EXT2_FT_BLKDEV    4     /* 块设备 */
 #define EXT2_FT_FIFO      5     /* FIFO */
 #define EXT2_FT_SOCK      6     /* 套接字 */
 #define EXT2_FT_SYMLINK   7     /* 符号链接 */
 
 /* Ext2间接块级别 */
 #define EXT2_NDIR_BLOCKS      12                      /* 直接块数 */
 #define EXT2_IND_BLOCK        EXT2_NDIR_BLOCKS        /* 单间接块 */
 #define EXT2_DIND_BLOCK       (EXT2_IND_BLOCK + 1)    /* 双间接块 */
 #define EXT2_TIND_BLOCK       (EXT2_DIND_BLOCK + 1)   /* 三间接块 */
 #define EXT2_N_BLOCKS         (EXT2_TIND_BLOCK + 1)   /* 总块数 */
 
 /* Ext2超级块结构 */
 struct ext2_super_block {
		 uint32 s_inodes_count;          /* Inodes数量 */
		 uint32 s_blocks_count;          /* 块数量 */
		 uint32 s_r_blocks_count;        /* 保留块数量 */
		 uint32 s_free_blocks_count;     /* 空闲块数量 */
		 uint32 s_free_inodes_count;     /* 空闲inode数量 */
		 uint32 s_first_data_block;      /* 第一个数据块 */
		 uint32 s_log_block_size;        /* 块大小 = 1024 << s_log_block_size */
		 uint32 s_log_frag_size;         /* 片段大小 */
		 uint32 s_blocks_per_group;      /* 每组块数 */
		 uint32 s_frags_per_group;       /* 每组片段数 */
		 uint32 s_inodes_per_group;      /* 每组inode数 */
		 uint32 s_mtime;                 /* 最后挂载时间 */
		 uint32 s_wtime;                 /* 最后写入时间 */
		 uint16 s_mnt_count;             /* 挂载次数 */
		 uint16 s_max_mnt_count;         /* 最大挂载次数 */
		 uint16 s_magic;                 /* 魔数 */
		 uint16 s_state;                 /* 文件系统状态 */
		 uint16 s_errors;                /* 错误处理方法 */
		 uint16 s_minor_rev_level;       /* 小版本号 */
		 uint32 s_lastcheck;             /* 最后检查时间 */
		 uint32 s_checkinterval;         /* 检查间隔 */
		 uint32 s_creator_os;            /* 创建者操作系统 */
		 uint32 s_rev_level;             /* 修订版本 */
		 uint16 s_def_resuid;            /* 默认保留块UID */
		 uint16 s_def_resgid;            /* 默认保留块GID */
		 /* EXT2_DYNAMIC_REV 特有的 */
		 uint32 s_first_ino;             /* 第一个非保留inode */
		 uint16 s_inode_size;            /* inode结构大小 */
		 uint16 s_block_group_nr;        /* 此超级块的块组 */
		 uint32 s_feature_compat;        /* 兼容特性集 */
		 uint32 s_feature_incompat;      /* 不兼容特性集 */
		 uint32 s_feature_ro_compat;     /* 只读兼容特性集 */
		 uint8  s_uuid[16];              /* 128位卷ID */
		 char   s_volume_name[16];       /* 卷名 */
		 char   s_last_mounted[64];      /* 最后挂载点路径 */
		 uint32 s_algorithm_usage_bitmap; /* 压缩算法 */
		 /* 性能提示 */
		 uint8  s_prealloc_blocks;       /* 预分配块数 */
		 uint8  s_prealloc_dir_blocks;   /* 预分配目录块数 */
		 uint16 s_padding1;
		 /* 日志支持 */
		 uint8  s_journal_uuid[16];      /* 日志超级块的UUID */
		 uint32 s_journal_inum;          /* 日志文件inode号 */
		 uint32 s_journal_dev;           /* 日志文件设备号 */
		 uint32 s_last_orphan;           /* 孤儿inode列表头 */
		 uint32 s_hash_seed[4];          /* 目录哈希种子 */
		 uint8  s_def_hash_version;      /* 默认哈希版本 */
		 uint8  s_reserved_char_pad;
		 uint16 s_reserved_word_pad;
		 uint32 s_default_mount_opts;    /* 默认挂载选项 */
		 uint32 s_first_meta_bg;         /* 第一个元数据块组 */
		 uint32 s_reserved[190];         /* 填充到1024字节 */
 };
 
 /* Ext2组描述符 */
 struct ext2_group_desc {
		 uint32 bg_block_bitmap;        /* 块位图块 */
		 uint32 bg_inode_bitmap;        /* inode位图块 */
		 uint32 bg_inode_table;         /* inode表起始块 */
		 uint16 bg_free_blocks_count;   /* 空闲块数量 */
		 uint16 bg_free_inodes_count;   /* 空闲inode数量 */
		 uint16 bg_used_dirs_count;     /* 目录数量 */
		 uint16 bg_pad;
		 uint32 bg_reserved[3];
 };
 
 /* Ext2 inode结构 */
 struct ext2_inode {
		 uint16 i_mode;                 /* 文件模式 */
		 uint16 i_uid;                  /* 低16位用户ID */
		 uint32 i_size;                 /* 大小（字节） */
		 uint32 i_atime;                /* 访问时间 */
		 uint32 i_ctime;                /* 创建时间 */
		 uint32 i_mtime;                /* 修改时间 */
		 uint32 i_dtime;                /* 删除时间 */
		 uint16 i_gid;                  /* 低16位组ID */
		 uint16 i_links_count;          /* 链接计数 */
		 uint32 i_blocks;               /* 块计数（512字节） */
		 uint32 i_flags;                /* 文件标志 */
		 uint32 i_osd1;                 /* OS依赖1 */
		 uint32 i_block[EXT2_N_BLOCKS]; /* 指向块的指针 */
		 uint32 i_generation;           /* 文件版本（NFS） */
		 uint32 i_file_acl;             /* 文件ACL */
		 uint32 i_dir_acl;              /* 目录ACL */
		 uint32 i_faddr;                /* 片段地址 */
		 uint8  i_osd2[12];             /* OS依赖2 */
 };
 
 /* Ext2目录项结构 */
 struct ext2_dir_entry {
		 uint32 inode;                  /* Inode号 */
		 uint16 rec_len;                /* 目录项长度 */
		 uint8  name_len;               /* 名称长度 */
		 uint8  file_type;              /* 文件类型 */
		 char   name[255];              /* 文件名 */
 };
 
 /* Ext2文件系统信息 */
 struct ext2_fs_info {
		 struct ext2_super_block *sb;         /* 超级块 */
		 struct ext2_group_desc *group_desc;  /* 组描述符 */
		 uint32 blocksize;                    /* 块大小 */
		 uint32 inodes_per_block;             /* 每块inode数 */
		 uint32 groups_count;                 /* 块组数量 */
		 struct buffer_head *sbh;             /* 超级块缓冲区 */
		 struct buffer_head *group_desc_bh;   /* 组描述符缓冲区 */
 };
 
 /* Ext2 inode信息 */
 struct ext2_inode_info {
		 uint32 i_data[EXT2_N_BLOCKS];        /* 块指针 */
		 uint32 i_flags;                      /* 文件标志 */
		 uint32 i_file_acl;                   /* 文件ACL */
		 uint32 i_dir_acl;                    /* 目录ACL */
		 uint32 i_dtime;                      /* 删除时间 */
		 uint32 i_block_group;                /* 文件的块组号 */
		 uint32 i_state;                      /* 动态状态标志 */
 };
 
 /* 函数声明 */
 
 /**
	* 挂载Ext2文件系统
	* 
	* @param fs_type 文件系统类型
	* @param flags 挂载标志
	* @param dev_name 设备名
	* @param data 私有数据
	* @return 成功返回超级块，失败返回错误指针
	*/
 struct super_block *ext2_mount(struct file_system_type *fs_type,
															int flags, const char *dev_name, void *data);
 
 /**
	* 读取Ext2超级块
	* 
	* @param sb 超级块
	* @param data 私有数据
	* @param silent 是否静默
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_fill_super(struct super_block *sb, void *data, int silent);
 
 /**
	* 注册Ext2文件系统
	* 
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_init(void);
 
 /**
	* 注销Ext2文件系统
	*/
 void ext2_exit(void);
 
 /**
	* 根据inode号读取inode结构
	* 
	* @param sb 超级块
	* @param ino inode号
	* @param bh 返回包含inode的缓冲区
	* @return inode结构的指针，失败返回NULL
	*/
 struct ext2_inode *ext2_get_inode(struct super_block *sb,
																 unsigned int ino, struct buffer_head **bh);
 
 /**
	* 分配新的inode
	* 
	* @param dir 父目录inode
	* @param mode 创建模式
	* @return 成功返回新inode，失败返回NULL
	*/
 struct inode *ext2_new_inode(struct inode *dir, int mode);
 
 /**
	* 分配新的块
	* 
	* @param sb 超级块
	* @param goal 期望分配的块号
	* @param bh 返回包含块位图的缓冲区
	* @return 成功返回新块号，失败返回0
	*/
 uint32 ext2_new_block(struct super_block *sb, uint32 goal, struct buffer_head **bh);
 
 /**
	* 创建常规文件
	* 
	* @param dir 父目录inode
	* @param dentry 目录项
	* @param mode 创建模式
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_create(struct inode *dir, struct dentry *dentry, int mode);
 
 /**
	* 创建符号链接
	* 
	* @param dir 父目录inode
	* @param dentry 目录项
	* @param symname 符号链接目标
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_symlink(struct inode *dir, struct dentry *dentry, const char *symname);
 
 /**
	* 创建目录
	* 
	* @param dir 父目录inode
	* @param dentry 目录项
	* @param mode 创建模式
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_mkdir(struct inode *dir, struct dentry *dentry, int mode);
 
 /**
	* 读取目录
	* 
	* @param file 文件对象
	* @param dirent 目录项缓冲区
	* @param filldir 填充函数
	* @return 成功返回0，失败返回负错误码
	*/
 int ext2_readdir(struct file *file, void *dirent,
								int (*filldir)(void *, const char *, int, uint64, uint64, unsigned));
 
 #endif /* _KERNEL_EXT2_H_ */