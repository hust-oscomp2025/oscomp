cmake_minimum_required(VERSION 3.10)
project(oscomp-dev C ASM)

add_subdirectory(user)
add_subdirectory(kernel)
add_subdirectory(vendor)


# 定义整体构建目标
add_custom_target(
    oskernel ALL
    DEPENDS riscv-pke create_fs_image
    COMMENT "Building complete OS kernel with user programs and system image"
)

# 构建完成后的消息
add_custom_command(
    TARGET oskernel
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "OS Kernel build complete!"
    COMMAND ${CMAKE_COMMAND} -E echo "Filesystem image created at: ${CMAKE_BINARY_DIR}/rootfs.img"
)

# 添加一个快速构建目标，不包括文件系统镜像
add_custom_target(
    quick_build
    DEPENDS kernel_image user_programs
    COMMENT "Building OS kernel and user programs without filesystem image"
)