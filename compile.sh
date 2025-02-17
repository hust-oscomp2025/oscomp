#!/bin/bash

# 清理旧的构建文件
rm -rf logs
mkdir ./logs
make clean

# 清空 make.log 文件
> ./logs/make.log

# 提示用户编译输出已重定向到 make.log 文件
echo "Makefile debug output is being redirected to './logs/make.log'."
echo "Compiler debug output is being redirected to './logs/compiler.log'."
# 运行 make，并将输出重定向到 make.log 文件
make >> ./logs/make.log
