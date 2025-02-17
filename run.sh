#!/bin/bash

# 读取 Makefile 中的 USER_TARGET 变量
USER_TARGET=$(make -f Makefile -s -C . print_user_target)

# 检查 USER_TARGET 是否为空
if [ -z "$USER_TARGET" ]; then
    echo "Error: USER_TARGET is not defined in Makefile"
    exit 1
fi

# 清空日志文件
> ./logs/run.log

# 使用 spike 运行并替换目标文件
spike ./obj/riscv-pke "$USER_TARGET" >> ./logs/run.log

echo "Successfully executed the target: $USER_TARGET"
# 提示用户编译输出已重定向到 make.log 文件
echo "Program output is being redirected to './logs/run.log'."