#!/bin/bash

# Create build directory if it doesn't exist
# 删除旧的 build 目录（如果存在）
if [ -d "build" ]; then
    echo "Cleaning up existing build directory..."
    rm -rf build
fi
mkdir -p build
cd build

# Configure with CMake
cmake ..
# Build all targets
cmake --build  . # --verbose
cd ..

echo "Build complete. Binaries are in build/bin and build/hostfs_root/bin"
echo "Run with: cd build && cmake --build . --target run"