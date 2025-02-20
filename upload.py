import os
import paramiko
from scp import SCPClient

def ssh_connect(host, port, username, password):
    """建立 SSH 连接"""
    try:
        # 创建 SSH 客户端
        ssh_client = paramiko.SSHClient()
        # 自动接受主机密钥
        ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
        # 连接到远程主机
        ssh_client.connect(host, port=port, username=username, password=password)
        print(f"Successfully connected to {host}")
        return ssh_client
    except Exception as e:
        print(f"Connection failed: {e}")
        return None

def remove_files(ssh_client, remote_dir):
    """删除远程目录下的所有文件"""
    try:
        # 使用 SSH 执行远程命令删除文件
        stdin, stdout, stderr = ssh_client.exec_command(f"rm -rf {remote_dir}/*")
        # 打印输出和错误（如果有的话）
        output = stdout.read().decode()
        error = stderr.read().decode()
        if output:
            print(f"Output: {output}")
        if error:
            print(f"Error: {error}")
        print(f"Files in {remote_dir} have been removed.")
    except Exception as e:
        print(f"Failed to remove files: {e}")

def upload_files(ssh_client, local_dir, remote_dir):
    """将本地文件上传到远程服务器"""
    try:
        # 创建 SCP 客户端
        with SCPClient(ssh_client.get_transport()) as scp:
            # 获取当前目录下的所有文件
            files = os.listdir(local_dir)
            for file in files:
                local_file_path = os.path.join(local_dir, file)
                # 检查是否为文件
                if os.path.isfile(local_file_path):
                    remote_file_path = os.path.join(remote_dir, file)
                    # 上传文件到远程目录
                    scp.put(local_file_path, remote_file_path)
                    print(f"Uploaded {file} to {remote_file_path}")
    except Exception as e:
        print(f"File upload failed: {e}")

def main():
    host = "47.96.157.89"  # 远程主机地址
    port = 51309  # SSH 端口，通常为 22
    username = "root"  # 远程主机用户名
    password = "Pw1_jxlJG9wb0BM7"  # 远程主机密码
    local_dir = "."  # 当前目录
    remote_dir = "/data/workspace/myshixun"  # 远程目录路径

    # 连接到远程主机
    ssh_client = ssh_connect(host, port, username, password)
    if ssh_client:
        # 删除远程目录中的所有文件
        remove_files(ssh_client, remote_dir)
        # 上传本地文件到远程目录
        upload_files(ssh_client, local_dir, remote_dir)
        # 关闭 SSH 连接
        ssh_client.close()

if __name__ == "__main__":
    main()
