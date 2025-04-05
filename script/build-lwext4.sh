#!/bin/bash

# 设置目标目录 - 指向 vendor/lwext4
TARGET_DIR="$(pwd)/vendor/lwext4"
# 设置目标库文件目录
DEST_LIB_DIR="$(pwd)/build/lib"

# 切换到目标目录
cd "$TARGET_DIR"

# 删除旧的 build 目录（如果存在）
if [ -d "build" ]; then
    echo "Cleaning up existing build directory..."
    rm -rf build
fi

# 创建并进入 build 目录
mkdir -p build
cd build

# 配置与构建
cmake ..
cmake --build . # --verbose

# 确保目标库目录存在
mkdir -p "$DEST_LIB_DIR"

# 复制生成的库文件到目标目录
if [ -f "lib/liblwext4.a" ]; then
    echo "Copying liblwext4.a to $DEST_LIB_DIR..."
    cp lib/liblwext4.a "$DEST_LIB_DIR/"
    echo "Library copied successfully."
else
    echo "Error: liblwext4.a not found in expected location."
    exit 1
fi

# 返回原目录
cd "$OLDPWD"

echo "Build complete. Binaries are in ${TARGET_DIR}/build/bin and ${TARGET_DIR}/build/hostfs_root/bin"
echo "Run with: cd ${TARGET_DIR}/build && cmake --build . --target run"
echo "Library file liblwext4.a has been copied to $DEST_LIB_DIR"