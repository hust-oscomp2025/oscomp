#!/bin/bash

# 获取当前分支名称
CURRENT_BRANCH="lab1_2_exception"

# 获取前一个分支的名称（假设你有最近的两个分支）
PREVIOUS_BRANCH="lab1_1_syscall"

# 如果前一个分支不存在，给出提示并退出
if [ -z "$PREVIOUS_BRANCH" ]; then
    echo "Error: No previous branch found. Could not determine the branch to merge."
    exit 1
fi

echo "Current branch: $CURRENT_BRANCH"
echo "Previous branch: $PREVIOUS_BRANCH"

# 切换到前一个分支并拉取最新的更改
# git checkout "$PREVIOUS_BRANCH"
# git pull

# 切换回当前分支并合并前一个分支的更改
# git checkout "$CURRENT_BRANCH"
git merge "$PREVIOUS_BRANCH"

echo "Successfully merged changes from '$PREVIOUS_BRANCH' into '$CURRENT_BRANCH'."
