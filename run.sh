#!/bin/bash

# 读取 Makefile 中的 USER_TARGET 变量
SPIKE_COMMAND=$(make -f Makefile -s -C . print_spike_command)

# 检查 USER_TARGET 是否为空
if [ -z "$SPIKE_COMMAND" ]; then
    echo "Error: USER_TARGET is not defined in Makefile"
    exit 1
fi

# 清空日志文件
> ./logs/run.log
echo $SPIKE_COMMAND

# 使用 spike 运行并替换目标文件
spike "$SPIKE_COMMAND" >> ./logs/run.log

echo "Successfully executed the target: $SPIKE_COMMAND"
# 提示用户编译输出已重定向到 make.log 文件
echo "Program output is being redirected to './logs/run.log'."