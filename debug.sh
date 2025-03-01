#!/bin/bash
# 释放资源
lsof -ti:9824 | xargs kill -9
lsof -ti:3333 | xargs kill -9
lsof -ti:6666 | xargs kill -9
pkill spike -9
pkill openocd -9
pkill cpptools-srv -9
# 执行make clean和make任务，可以直接换成对应的命令
# source ./compile.sh
# 设置变量
PKE="./obj/riscv-pke"
# 读取 Makefile 中的 USER_TARGET 变量
USER_TARGET="./obj/app_wait"

# 检查 USER_TARGET 是否为空
if [ -z "$USER_TARGET" ]; then
    echo "Error: USER_TARGET is not defined in Makefile"
    exit 1
fi
PROGRAM="$PKE $USER_TARGET"
SPIKE_PORT="9824"
OPENOCD_CFG="spike.cfg"
GDB_CMD="riscv64-unknown-elf-gdb"
TARGET_REMOTE="localhost:3333"

# 1. 运行 Spike，并监听远程 Bitbang 连接
echo "Starting Spike with remote bitbang on port $SPIKE_PORT..."
spike --rbb-port=$SPIKE_PORT --halted -m0x80000000:0x90000000 $PROGRAM &

# 确保 Spike 启动成功
sleep 2

# 2. 启动 OpenOCD 使用指定配置文件
echo "Starting OpenOCD with configuration file $OPENOCD_CFG..."
openocd -f $OPENOCD_CFG -c "reset halt" &

# 确保 OpenOCD 启动成功
sleep 3

# 3. 启动 GDB，连接到目标远程调试
echo "Starting GDB and connecting to $TARGET_REMOTE..."
riscv64-unknown-elf-gdb -ex "target extended-remote $TARGET_REMOTE" \
                        -ex "b malloc" \
                        -ex "c " \
                        $PROGRAM
                        
                        # -ex "add-symbol-file $USER_PROGRAM 0x0" \
                        #-ex "b main" \ # -ex "symbol-file $USER_PROGRAM" \
                        # -ex "b _mentry" \
# 4. 结束后清理后台进程
echo "Debugging session completed. Cleaning up..."
lsof -ti:9824 | xargs kill -9
lsof -ti:3333 | xargs kill -9
lsof -ti:6666 | xargs kill -9