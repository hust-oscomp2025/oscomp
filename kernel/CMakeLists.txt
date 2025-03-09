cmake_minimum_required(VERSION 3.10)
project(riscv-pke C ASM)

# 设置 RISC-V 交叉编译工具链
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

# Compiler flags
set(CMAKE_C_FLAGS "-Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE -fno-omit-frame-pointer")

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

)

# 设置链接脚本
set_target_properties(riscv-pke PROPERTIES LINK_FLAGS "-T ${CMAKE_SOURCE_DIR}/kernel/kernel.lds")



set(DEBUG_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
# ✅ 生成 objdump 和 ELF 解析文件
add_custom_command(
    TARGET riscv-pke
    POST_BUILD
    COMMAND mkdir -p ${DEBUG_OUTPUT_DIRECTORY}  # 确保 debug 目录存在
    COMMAND riscv64-unknown-elf-objdump -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.dump
    COMMAND riscv64-unknown-elf-readelf -a ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.elfinfo
    COMMENT "Generating objdump and ELF analysis for riscv-pke in ${DEBUG_OUTPUT_DIRECTORY}"
)













