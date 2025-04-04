#!/bin/bash

# 获取容器自身的 IP 地址
CONTAINER_IP=$(hostname -I | awk '{print $1}')

if [ -z "$CONTAINER_IP" ]; then
    echo "无法获取容器 IP 地址。"
    exit 1
fi

echo "容器 IP 地址: $CONTAINER_IP"

# 设置 VSCode 配置文件路径
CONFIG_FILE="/workspace/.vscode/launch.json"  # 根据你的挂载情况调整路径

# 检查文件是否存在
if [ ! -f "$CONFIG_FILE" ]; then
    echo "找不到配置文件: $CONFIG_FILE"
    echo "请确保路径正确，并且.vscode目录已经挂载到容器中。"
    exit 1
fi

# 使用 jq 工具更新 JSON 配置文件（如果安装了 jq）
if command -v jq &> /dev/null; then
    # 创建临时文件
    TMP_FILE=$(mktemp)
    
    # 读取原始 JSON 并更新
    jq --arg ip "$CONTAINER_IP" '(.configurations[] | select(.name == "LLDB Debug")).initCommands |= 
        map(if startswith("platform connect") then "platform connect connect:\($ip):1234" else . end)' "$CONFIG_FILE" > "$TMP_FILE"
    
    # 写回原文件
    mv "$TMP_FILE" "$CONFIG_FILE"
    
    echo "已使用 jq 更新 VSCode 配置文件中的容器 IP 地址。"
else
    # 如果没有 jq，使用 sed 进行简单替换
    sed -i "s/connect:[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+:1234/connect:$CONTAINER_IP:1234/g" "$CONFIG_FILE"
    sed -i "s/connect:localhost:1234/connect:$CONTAINER_IP:1234/g" "$CONFIG_FILE"
    sed -i "s/connect:127\.0\.0\.1:1234/connect:$CONTAINER_IP:1234/g" "$CONFIG_FILE"
    
    echo "已使用 sed 更新 VSCode 配置文件中的容器 IP 地址。"
fi

# 验证更新
echo "更新后的连接字符串:"
grep -A 1 "platform connect" "$CONFIG_FILE"