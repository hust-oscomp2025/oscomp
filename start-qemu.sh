#!/bin/bash
set -e  # 遇到错误立即退出

# 定义日志文件
LOG_FILE="/tmp/qemu-riscv-debug.log"

# 清理旧进程
echo "Cleaning up old QEMU processes..."
pkill -f 'qemu-system-riscv64.*-s -S' &>/dev/null || true

# 确保内核文件存在
KERNEL_PATH="$1"
if [ ! -f "$KERNEL_PATH" ]; then
    echo "错误: 内核文件 $KERNEL_PATH 不存在!"
    exit 1
fi

# 显式使用前台方式启动QEMU，重定向输出到日志文件
echo "Starting QEMU with GDB server on port 1234..."
qemu-system-riscv64 -machine virt -nographic -bios default -kernel "$KERNEL_PATH" -s -S > "$LOG_FILE" 2>&1 &
QEMU_PID=$!

# 给QEMU一点时间启动
sleep 2

# 检查QEMU是否正在运行
if ! ps -p $QEMU_PID > /dev/null; then
    echo "错误: QEMU进程启动失败! 查看日志: $LOG_FILE"
    exit 1
fi

# 检查端口是否打开
if command -v nc &> /dev/null; then
    if ! nc -z localhost 1234; then
        echo "错误: 端口1234没有打开。QEMU可能没有正确启动GDB服务器。"
        echo "QEMU进程 (PID: $QEMU_PID) 将被终止。"
        kill $QEMU_PID
        exit 1
    fi
fi

echo "QEMU 成功启动 (PID: $QEMU_PID)"
echo "GDB 服务器正在监听端口 1234"
echo "查看QEMU输出: cat $LOG_FILE"