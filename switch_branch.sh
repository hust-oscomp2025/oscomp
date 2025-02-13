#!/bin/bash

# Set the target branch name here. Modify as needed.
TARGET_BRANCH="lab1_3_irq"

# Check if the working directory is clean (no uncommitted changes)
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Uncommitted changes detected. Please commit or stash your changes before switching branches."
    exit 1
fi

echo "Working directory is clean. Proceeding with branch switch..."

# Fetch the latest updates from remote
git fetch

# Check if the target branch exists on remote
if ! git ls-remote --exit-code --heads origin "$TARGET_BRANCH" > /dev/null; then
    echo "Error: Remote branch '$TARGET_BRANCH' does not exist. Please check the branch name."
    exit 1
fi

# If the local branch doesn't exist, create it tracking the remote branch; otherwise, switch and update it.
if ! git show-ref --verify --quiet "refs/heads/$TARGET_BRANCH"; then
    echo "Creating and switching to local branch '$TARGET_BRANCH' tracking 'origin/$TARGET_BRANCH'..."
    git checkout -b "$TARGET_BRANCH" "origin/$TARGET_BRANCH"
else
    echo "Local branch '$TARGET_BRANCH' exists. Switching and pulling latest changes..."
    git checkout "$TARGET_BRANCH"
    git pull
fi

echo "Successfully switched to branch '$TARGET_BRANCH'."
