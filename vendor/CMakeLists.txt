cmake_minimum_required(VERSION 3.10)

# 第三方库构建配置
message(STATUS "Configuring third-party libraries")

# lwext4库 - 必需
add_subdirectory(lwext4)

# musl库 - 必需
add_subdirectory(musl)

# busybox - 必需，并做成镜像
add_subdirectory(busybox)

# 其他可能的第三方库
# add_subdirectory(其他库名称)

# 创建系统镜像目录结构
set(SYSROOT_DIR ${CMAKE_BINARY_DIR}/sysroot)
set(HOSTFS_DIR ${CMAKE_BINARY_DIR}/hostfs)

# 创建一个整合目标，可以一次性构建所有第三方库
add_custom_target(
    build_all_vendor
    DEPENDS lwext4 musl_lib busybox_build
    COMMENT "Building all third-party libraries"
)

# 创建系统镜像
add_custom_target(
    create_system_image
    DEPENDS build_all_vendor
    
    # 创建基本目录结构
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/sbin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/etc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/usr/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/usr/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/usr/sbin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/dev
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/proc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/sys
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/tmp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/var
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/root
    
    # 将busybox安装到系统镜像
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/vendor/busybox/install/bin ${HOSTFS_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/vendor/busybox/install/sbin ${HOSTFS_DIR}/sbin
    COMMAND test -d ${CMAKE_BINARY_DIR}/vendor/busybox/install/usr/bin && ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/vendor/busybox/install/usr/bin ${HOSTFS_DIR}/usr/bin || echo "usr/bin 不存在，跳过"
    COMMAND test -d ${CMAKE_BINARY_DIR}/vendor/busybox/install/usr/sbin && ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/vendor/busybox/install/usr/sbin ${HOSTFS_DIR}/usr/sbin || echo "usr/sbin 不存在，跳过"
    
    # 将musl库复制到系统镜像
    # COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/vendor/musl/install/lib ${HOSTFS_DIR}/lib
    
    # 复制其他需要包含的文件和用户程序
    COMMAND test -d ${CMAKE_BINARY_DIR}/user && ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/user ${HOSTFS_DIR}/bin || echo "没有找到用户程序"
    
    # 创建基本的配置文件
    COMMAND echo "/bin/sh" > ${HOSTFS_DIR}/etc/shells
    COMMAND echo "root::0:0:root:/root:/bin/sh" > ${HOSTFS_DIR}/etc/passwd
    COMMAND echo "root:x:0:" > ${HOSTFS_DIR}/etc/group
    COMMAND echo "::sysinit:/etc/init.d/rcS" > ${HOSTFS_DIR}/etc/inittab
    COMMAND echo "::respawn:-/bin/sh" >> ${HOSTFS_DIR}/etc/inittab
    COMMAND echo "::restart:/sbin/init" >> ${HOSTFS_DIR}/etc/inittab
    COMMAND echo "::shutdown:/bin/umount -a -r" >> ${HOSTFS_DIR}/etc/inittab
    
    # 创建初始化脚本
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HOSTFS_DIR}/etc/init.d
    COMMAND echo "#!/bin/sh" > ${HOSTFS_DIR}/etc/init.d/rcS
    COMMAND echo "mount -t proc none /proc" >> ${HOSTFS_DIR}/etc/init.d/rcS
    COMMAND echo "mount -t sysfs none /sys" >> ${HOSTFS_DIR}/etc/init.d/rcS
    COMMAND echo "mount -t devtmpfs none /dev" >> ${HOSTFS_DIR}/etc/init.d/rcS
    COMMAND chmod +x ${HOSTFS_DIR}/etc/init.d/rcS
    
    COMMENT "Creating system image with busybox and required libraries in hostfs"
)

# 创建ext4文件系统镜像（使用独立脚本）
add_custom_target(
    create_fs_image
    DEPENDS create_system_image
    
    # 调用外部脚本来创建镜像
    COMMAND chmod +x ${CMAKE_SOURCE_DIR}/script/create_filesystem.sh
    COMMAND ${CMAKE_SOURCE_DIR}/script/create_filesystem.sh 
            ${HOSTFS_DIR} 
            ${CMAKE_BINARY_DIR}/disk_img/rootfs.img 
            128  # 镜像大小(MB)，增加到128MB
            0    # 不强制重建，如果镜像已存在且比源文件新则跳过
    
    COMMENT "Creating ext4 filesystem image with system files"
)

# 导出镜像路径变量
set(ROOTFS_IMAGE ${CMAKE_BINARY_DIR}/disk_img/rootfs.img PARENT_SCOPE)
set(HOSTFS_DIR ${HOSTFS_DIR} PARENT_SCOPE)