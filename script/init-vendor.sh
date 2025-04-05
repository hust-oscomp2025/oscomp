#!/bin/bash
# 初始化vendor目录并设置所有第三方库

set -e

# 设置目录
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
VENDOR_DIR="$PROJECT_ROOT/vendor"

# 颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 创建vendor目录（如果不存在）
mkdir -p "$VENDOR_DIR"

# 复制CMakeLists.txt到vendor目录
cp "${SCRIPT_DIR}/cmake/vendor_CMakeLists.txt" "${VENDOR_DIR}/CMakeLists.txt"

echo -e "${GREEN}Vendor目录已创建: ${VENDOR_DIR}${NC}"

# 克隆lwext4
if [ ! -d "${VENDOR_DIR}/lwext4" ]; then
    echo -e "${YELLOW}正在克隆lwext4...${NC}"
    git clone https://github.com/gkostka/lwext4.git "${VENDOR_DIR}/lwext4"
    cp "${SCRIPT_DIR}/cmake/lwext4_CMakeLists.txt" "${VENDOR_DIR}/lwext4/CMakeLists.txt"
    echo -e "${GREEN}lwext4已克隆并配置${NC}"
else
    echo -e "${YELLOW}lwext4已存在，跳过克隆${NC}"
    cp "${SCRIPT_DIR}/cmake/lwext4_CMakeLists.txt" "${VENDOR_DIR}/lwext4/CMakeLists.txt"
fi

# 克隆musl
if [ ! -d "${VENDOR_DIR}/musl" ]; then
    echo -e "${YELLOW}正在克隆musl...${NC}"
    git clone https://git.musl-libc.org/git/musl "${VENDOR_DIR}/musl"
    cp "${SCRIPT_DIR}/cmake/musl_CMakeLists.txt" "${VENDOR_DIR}/musl/CMakeLists.txt"
    echo -e "${GREEN}musl已克隆并配置${NC}"
else
    echo -e "${YELLOW}musl已存在，跳过克隆${NC}"
    cp "${SCRIPT_DIR}/cmake/musl_CMakeLists.txt" "${VENDOR_DIR}/musl/CMakeLists.txt"
fi

# 克隆busybox
if [ ! -d "${VENDOR_DIR}/busybox" ]; then
    echo -e "${YELLOW}正在克隆busybox...${NC}"
    git clone https://git.busybox.net/busybox "${VENDOR_DIR}/busybox"
    cp "${SCRIPT_DIR}/cmake/busybox_CMakeLists.txt" "${VENDOR_DIR}/busybox/CMakeLists.txt"
    echo -e "${GREEN}busybox已克隆并配置${NC}"
else
    echo -e "${YELLOW}busybox已存在，跳过克隆${NC}"
    cp "${SCRIPT_DIR}/cmake/busybox_CMakeLists.txt" "${VENDOR_DIR}/busybox/CMakeLists.txt"
fi

# 创建cmake配置目录
mkdir -p "${SCRIPT_DIR}/cmake"

# 复制所有CMakeLists.txt到cmake目录备份
cp "${VENDOR_DIR}/CMakeLists.txt" "${SCRIPT_DIR}/cmake/vendor_CMakeLists.txt"
cp "${VENDOR_DIR}/lwext4/CMakeLists.txt" "${SCRIPT_DIR}/cmake/lwext4_CMakeLists.txt"
cp "${VENDOR_DIR}/musl/CMakeLists.txt" "${SCRIPT_DIR}/cmake/musl_CMakeLists.txt"
cp "${VENDOR_DIR}/busybox/CMakeLists.txt" "${SCRIPT_DIR}/cmake/busybox_CMakeLists.txt"

echo -e "${GREEN}初始化完成！第三方库已准备好，可以进行CMake构建。${NC}"
echo -e "${YELLOW}请运行以下命令开始构建:${NC}"
echo -e "mkdir -p build && cd build"
echo -e "cmake .. && make"