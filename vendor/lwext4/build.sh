#!/bin/bash

# 设置脚本在发生错误时退出
set -e

# 删除旧的 build 目录（如果存在）
if [ -d "build" ]; then
    echo "Cleaning up existing build directory..."
    rm -rf build
fi

# 创建新的 build 目录并进入
mkdir build && cd build

# 运行 CMake 配置
echo "Configuring project with CMake..."
cmake ..

# 构建项目
echo "Building project..."
cmake --build .

echo "Build completed successfully!"
