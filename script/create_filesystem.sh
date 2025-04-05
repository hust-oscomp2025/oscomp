#!/bin/bash
# 创建文件系统镜像的脚本 - 适用于Docker环境
# 用法: ./create_filesystem.sh <源目录> <输出镜像> [镜像大小MB] [force]

set -e

# 参数检查
SOURCE_DIR="$1"
OUTPUT_IMG="$2"
SIZE_MB="${3:-128}"  # 默认128MB，增加默认大小
FORCE="${4:-0}"      # 是否强制重建，默认否

if [ -z "$SOURCE_DIR" ] || [ -z "$OUTPUT_IMG" ]; then
    echo "用法: $0 <源目录> <输出镜像> [镜像大小MB] [force]"
    echo "例如: $0 ./hostfs ./disk_img/rootfs.img 128 0"
    echo "参数force为1时会强制重建镜像，即使已存在"
    exit 1
fi

# 确保输出目录存在
OUTPUT_DIR=$(dirname "$OUTPUT_IMG")
mkdir -p "$OUTPUT_DIR"

# 检查镜像是否已存在
if [ -f "$OUTPUT_IMG" ] && [ "$FORCE" != "1" ]; then
    # 计算文件更改时间
	echo "镜像文件已存在: $OUTPUT_IMG"
	echo "如需重建，请使用force=1参数或删除现有镜像"
	exit 0
    # if [ -d "$SOURCE_DIR" ]; then
    #     NEWEST_SOURCE=$(find "$SOURCE_DIR" -type f -printf '%T@ %p\n' | sort -n | tail -1 | cut -f1 -d' ')
    #     IMAGE_TIME=$(stat -c %Y "$OUTPUT_IMG")
        
    #     # 如果没有找到源文件或镜像比所有源文件都新，则跳过
    #     # if [ -z "$NEWEST_SOURCE" ] || [ "$IMAGE_TIME" -gt "$NEWEST_SOURCE" ]; then
	# 	if [ -z "$NEWEST_SOURCE" ]; then
    #         echo "镜像文件已存在: $OUTPUT_IMG"
    #         echo "如需重建，请使用force=1参数或删除现有镜像"
    #         exit 0
    #     fi
    # else
    #     echo "镜像文件已存在且源目录不存在，跳过: $OUTPUT_IMG"
    #     exit 0
    # fi
fi

echo "===== 创建文件系统镜像 ====="
echo "源目录: $SOURCE_DIR"
echo "输出镜像: $OUTPUT_IMG"
echo "镜像大小: ${SIZE_MB}MB"

# 创建tarball作为备选
echo "创建tar备份..."
tar -czf "${OUTPUT_IMG%.img}.tar.gz" -C "$SOURCE_DIR" .

# 计算源目录的大小，确保镜像足够大
SOURCE_SIZE_KB=$(du -sk "$SOURCE_DIR" | cut -f1)
SOURCE_SIZE_MB=$((SOURCE_SIZE_KB / 1024))
REQUIRED_SIZE_MB=$((SOURCE_SIZE_MB * 2)) # 至少需要源目录两倍大小

if [ "$SIZE_MB" -lt "$REQUIRED_SIZE_MB" ]; then
    echo "警告: 镜像大小($SIZE_MB MB)可能不足以容纳源数据($SOURCE_SIZE_MB MB)"
    echo "自动增加镜像大小至 $REQUIRED_SIZE_MB MB"
    SIZE_MB=$REQUIRED_SIZE_MB
fi

# 尝试方法1: 使用genext2fs (不需要挂载或root权限)
if command -v genext2fs &>/dev/null; then
    echo "使用genext2fs创建ext4镜像..."
    # 计算Block大小和数量，inode数量基于文件数的估计
    BLOCKS=$((SIZE_MB * 1024)) # 1MB = 1024个1KB的块
    FILE_COUNT=$(find "$SOURCE_DIR" -type f | wc -l)
    INODE_COUNT=$((FILE_COUNT * 2)) # 至少是文件数的两倍
    
    # 确保足够的inode
    if [ "$INODE_COUNT" -lt 16384 ]; then
        INODE_COUNT=16384
    fi
    
    echo "使用 $BLOCKS 块 (每块1KB) 和 $INODE_COUNT inodes"
    
    genext2fs -d "$SOURCE_DIR" -b $BLOCKS -i 1024 -N $INODE_COUNT "$OUTPUT_IMG"
    
    # 如果支持，转换为ext4格式
    if command -v tune2fs &>/dev/null; then
        echo "添加ext4日志特性..."
        tune2fs -j "$OUTPUT_IMG" >/dev/null 2>&1
    fi
    
    # 检查文件系统
    if command -v e2fsck &>/dev/null; then
        echo "检查文件系统..."
        e2fsck -fy "$OUTPUT_IMG" >/dev/null 2>&1 || true
    fi
    
    echo "成功创建ext4镜像: $OUTPUT_IMG"
    exit 0
fi

# 尝试方法2: 使用较新版本的e2fsprogs中的mke2fs (更灵活的替代)
if command -v mke2fs &>/dev/null; then
    echo "使用mke2fs创建ext4镜像..."
    dd if=/dev/zero of="$OUTPUT_IMG" bs=1M count="$SIZE_MB"
    
    # 使用mke2fs创建文件系统
    mke2fs -t ext4 -d "$SOURCE_DIR" "$OUTPUT_IMG"
    if [ $? -eq 0 ]; then
        echo "成功创建ext4镜像: $OUTPUT_IMG"
        exit 0
    fi
fi

# 尝试方法3: 使用mkfs.ext4的-d选项（较新版本支持）
echo "尝试使用mkfs.ext4 -d选项..."
if dd if=/dev/zero of="$OUTPUT_IMG" bs=1M count="$SIZE_MB" && 
   mkfs.ext4 -q -F -d "$SOURCE_DIR" "$OUTPUT_IMG"; then
    echo "成功创建ext4镜像: $OUTPUT_IMG"
    exit 0
fi

# 如果前面的方法都失败，提示仍然有tarball可用
echo "注意: 未能创建ext4镜像，但tarball已创建: ${OUTPUT_IMG%.img}.tar.gz"
echo "你可以在有root权限的环境中使用以下命令来创建ext4镜像:"
echo "dd if=/dev/zero of=$OUTPUT_IMG bs=1M count=$SIZE_MB"
echo "mkfs.ext4 $OUTPUT_IMG"
echo "mkdir -p /mnt/temp"
echo "sudo mount $OUTPUT_IMG /mnt/temp"
echo "sudo tar -xf ${OUTPUT_IMG%.img}.tar.gz -C /mnt/temp"
echo "sudo umount /mnt/temp"

exit 1