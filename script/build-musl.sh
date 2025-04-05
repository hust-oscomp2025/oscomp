#!/bin/bash
set -e

# 配置变量
MUSL_SOURCE="$(pwd)/vendor/musl"
BUILD_DIR="$(pwd)/build"
JOBS=$(nproc 2>/dev/null || echo 2)

# 确保脚本在项目根目录运行
if [ ! -d "script" ] || [ ! -d "vendor/musl" ]; then
    echo "错误: 请在项目根目录运行此脚本，并确保vendor/musl目录存在"
    exit 1
fi

# 检查编译工具是否存在
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "错误: riscv64-unknown-elf-gcc 未找到，请安装RISC-V交叉编译工具链"
    exit 1
fi

echo "开始构建 musl for riscv64..."

# 准备构建目录
mkdir -p ${BUILD_DIR}

# 配置并编译
cd ${MUSL_SOURCE}

# 如果之前已经配置，清理一下
if [ -f "config.mak" ]; then
    make distclean
fi

# 配置
echo "配置 musl..."
./configure \
  --target=riscv64-unknown-elf \
  --disable-shared \
  CFLAGS="-g3 -O0 -fno-omit-frame-pointer -mabi=lp64d" \
  CC=riscv64-unknown-elf-gcc

# 编译
echo "编译 musl (使用 ${JOBS} 个作业)..."
make -j${JOBS}

# 复制静态库到项目目录
mkdir -p ${BUILD_DIR}/lib
cp lib/libc.a ${BUILD_DIR}/lib/

# 回到项目根目录
cd $(pwd)/../

echo "构建完成!"
echo "MUSL静态库已构建完成: ${BUILD_DIR}/lib/libc.a"