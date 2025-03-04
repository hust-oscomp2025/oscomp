#!/bin/bash

# 用法提示函数
usage() {
    echo "Usage: $0 <branch_to_merge>"
    echo "  <branch_to_merge>   合并的目标分支."
    git branch -r
    exit 1
}

# 如果没有提供目标合并分支名称作为命令行参数，则给出用法提示
if [ $# -ne 1 ]; then
    usage
fi

# 获取当前分支名称
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

# 目标合并分支名称，从命令行参数获取
TARGET_BRANCH="$1"

# 如果目标分支名称为空，给出提示并退出
if [ -z "$TARGET_BRANCH" ]; then
    echo "Error: No branch specified to merge."
    usage
fi

# 输出当前分支和目标分支信息
echo "Current branch: $CURRENT_BRANCH"
echo "Branch to merge: $TARGET_BRANCH"

# 切换到目标分支并拉取最新的更改
git checkout "$TARGET_BRANCH"
git pull

# 切换回当前分支并合并目标分支的更改
git checkout "$CURRENT_BRANCH"
git merge "$TARGET_BRANCH"

echo "Successfully merged changes from '$TARGET_BRANCH' into '$CURRENT_BRANCH'."
