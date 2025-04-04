#!/bin/bash
set -e

# 清理旧进程
echo "Cleaning up old QEMU processes..."
pkill -f 'qemu-system-riscv64.*-s -S' &>/dev/null || true

# 设置变量
KERNEL_PATH="$1"
LOG_FILE="/tmp/qemu-riscv-debug.log"
GDB_PORT="1234"

# 检查内核文件
if [ ! -f "$KERNEL_PATH" ]; then
    echo "错误: 内核文件 $KERNEL_PATH 不存在!"
    exit 1
fi

# 查找可用IP地址 (对Docker环境很重要)
# 在Docker环境中，localhost可能不会如预期工作，我们需要绑定到容器的实际IP
CONTAINER_IP=$(hostname -I | awk '{print $1}')
if [ -z "$CONTAINER_IP" ]; then
    CONTAINER_IP="0.0.0.0"  # 如果无法获取IP，则绑定到所有接口
fi

echo "使用IP地址: $CONTAINER_IP"

# 明确指定GDB服务器地址和端口 (不使用-s简写)
echo "启动QEMU，GDB服务器绑定到 $CONTAINER_IP:$GDB_PORT..."
qemu-system-riscv64 -machine virt -nographic -bios default \
    -kernel "$KERNEL_PATH" \
    -gdb tcp::$GDB_PORT,server,nowait \
    > "$LOG_FILE" 2>&1 &

QEMU_PID=$!

# 等待QEMU启动
sleep 2

# 验证QEMU进程
if ! ps -p $QEMU_PID > /dev/null; then
    echo "错误: QEMU进程启动失败! 查看日志: $LOG_FILE"
    cat "$LOG_FILE"
    exit 1
fi

# 显示网络连接情况
echo "检查网络连接状态:"
netstat -tulpn 2>/dev/null | grep $GDB_PORT || ss -tulpn | grep $GDB_PORT || echo "无法检测端口状态"

echo "QEMU 成功启动 (PID: $QEMU_PID)"
echo "GDB 服务器绑定到: $CONTAINER_IP:$GDB_PORT"
echo "尝试使用以下GDB命令连接:"
echo "  target remote $CONTAINER_IP:$GDB_PORT"
echo "查看QEMU输出: cat $LOG_FILE"