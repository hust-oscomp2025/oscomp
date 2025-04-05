cmake_minimum_required(VERSION 3.10)
project(riscv-pke C ASM)

# # 修改: 切换到 RISC-V Linux GNU 交叉编译工具链
# set(CMAKE_SYSTEM_NAME Linux)
# set(CMAKE_SYSTEM_PROCESSOR riscv64)
# set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
# set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++)
# set(CMAKE_ASM_COMPILER riscv64-linux-gnu-gcc)

# 设置 RISC-V 交叉编译工具链
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

# 修改: 调整编译标志以适应新工具链
set(CMAKE_C_FLAGS "-Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -nostartfiles -nostdinc -ffreestanding -T ${CMAKE_SOURCE_DIR}/kernel/kernel.lds -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -Wl,--no-relax -Wl,-z,notext -fno-delete-null-pointer-checks -fno-PIE -fno-omit-frame-pointer")

# 设置特殊的包含路径，确保使用工具链中的标准 POSIX 头文件
set(GNU_INCLUDE_PATH "/usr/riscv64-linux-gnu/include")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -isystem ${GNU_INCLUDE_PATH}")

# Apply the same flags to ASM
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 递归获取 kernel 目录及其所有子目录中的 C 源文件
file(GLOB_RECURSE KERNEL_SOURCES "**/*.c")
file(GLOB_RECURSE KERNEL_SOURCES "*.c")

# 递归获取 kernel 目录及其所有子目录中的 汇编（.S）源文件
file(GLOB_RECURSE KERNEL_ASM_SOURCES "**/*.S")
file(GLOB_RECURSE KERNEL_ASM_SOURCES "*.S")

add_executable(riscv-pke ${KERNEL_SOURCES} ${KERNEL_ASM_SOURCES})

target_include_directories(riscv-pke PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/
    # 添加需要的 POSIX 头文件路径
	"/usr/lib/gcc-cross/riscv64-linux-gnu/13/include/"
    ${GNU_INCLUDE_PATH}
    ${GNU_INCLUDE_PATH}/linux
    ${GNU_INCLUDE_PATH}/asm-generic
)

# 添加 lwext4 静态库 - 需要确保它也是用 riscv64-linux-gnu-gcc 编译的
add_library(lwext4 STATIC IMPORTED)
set_target_properties(lwext4 PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/kernel/lib/liblwext4.a)
# 链接 lwext4 静态库
target_link_libraries(riscv-pke PRIVATE lwext4)



set(DEBUG_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
# ✅ 生成 objdump 和 ELF 解析文件 - 也更新工具
add_custom_command(
    TARGET riscv-pke
    POST_BUILD
    COMMAND mkdir -p ${DEBUG_OUTPUT_DIRECTORY}  # 确保 debug 目录存在
    COMMAND riscv64-linux-gnu-objdump -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.dump
    COMMAND riscv64-linux-gnu-readelf -a ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.elfinfo
    COMMENT "Generating objdump and ELF analysis for riscv-pke in ${DEBUG_OUTPUT_DIRECTORY}"
)