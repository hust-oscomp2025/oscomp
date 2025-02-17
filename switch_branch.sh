#!/bin/bash

# 获取当前分支名称
CURRENT_BRANCH=$(git symbolic-ref --short HEAD)
echo "Current branch: $CURRENT_BRANCH."

# 检查是否提供了目标分支名称作为命令行参数
if [ -z "$1" ]; then
    echo "Usage: $0 <target_branch>"
    echo "\"git branch -r\" to show all remote branches."
    exit 1
fi

# 将命令行参数作为目标分支
TARGET_BRANCH="$1"

# 检查工作目录是否有未提交的更改
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Uncommitted changes detected. Please commit or stash your changes before switching branches."
    exit 1
fi

echo "Working directory is clean. Proceeding with branch switch..."

# 拉取远程更新
git fetch

# 检查目标分支是否存在于远程
if ! git ls-remote --exit-code --heads origin "$TARGET_BRANCH" > /dev/null; then
    echo "Error: Remote branch '$TARGET_BRANCH' does not exist. Please check the branch name."
    exit 1
fi

# 如果本地没有目标分支，创建并跟踪远程分支；否则切换并更新它
if ! git show-ref --verify --quiet "refs/heads/$TARGET_BRANCH"; then
    echo "Creating and switching to local branch '$TARGET_BRANCH' tracking 'origin/$TARGET_BRANCH'..."
    git checkout -b "$TARGET_BRANCH" "origin/$TARGET_BRANCH"
else
    echo "Local branch '$TARGET_BRANCH' exists. Switching and pulling latest changes..."
    git checkout "$TARGET_BRANCH"
    git pull
fi

echo "Successfully switched to branch '$TARGET_BRANCH'."
# git merge "$CURRENT_BRANCH"
# echo "Successfully merged branch '$CURRENT_BRANCH'."
