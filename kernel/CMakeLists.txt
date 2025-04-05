cmake_minimum_required(VERSION 3.10)
project(riscv-pke C ASM)

# 设置 RISC-V 交叉编译工具链
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

# 编译标志
set(CMAKE_C_FLAGS "-Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -nostartfiles -nostdinc -ffreestanding -T ${CMAKE_SOURCE_DIR}/kernel/kernel.lds -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -Wl,--no-relax -Wl,-z,notext -fno-delete-null-pointer-checks -fno-PIE -fno-omit-frame-pointer")

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

# 修改后的头文件包含路径，只保留include/和vendor/musl/include/
target_include_directories(riscv-pke PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    # ${CMAKE_SOURCE_DIR}/include/riscv64-linux-gnu
    # ${CMAKE_SOURCE_DIR}/include/gcc13
    ${CMAKE_SOURCE_DIR}/vendor/musl/include
	${CMAKE_SOURCE_DIR}/vendor/musl/arch/generic
	${CMAKE_SOURCE_DIR}/vendor/musl/arch/riscv64
	${CMAKE_SOURCE_DIR}/vendor/musl/obj/include
)

# 添加 lwext4 静态库
add_library(lwext4 STATIC IMPORTED)
set_target_properties(lwext4 PROPERTIES IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/liblwext4.a)

# 添加 libc.a 静态库
add_library(c STATIC IMPORTED)
set_target_properties(c PROPERTIES IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/lib/libc.a)

# 链接静态库
target_link_libraries(riscv-pke PRIVATE lwext4 c)

set(DEBUG_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
# 生成 objdump 和 ELF 解析文件
add_custom_command(
    TARGET riscv-pke
    POST_BUILD
    COMMAND mkdir -p ${DEBUG_OUTPUT_DIRECTORY}
    COMMAND riscv64-unknown-elf-objdump -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.dump
    COMMAND riscv64-unknown-elf-readelf -a ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/riscv-pke > ${DEBUG_OUTPUT_DIRECTORY}/riscv-pke.elfinfo
    COMMENT "Generating objdump and ELF analysis for riscv-pke in ${DEBUG_OUTPUT_DIRECTORY}"
)