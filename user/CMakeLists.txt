cmake_minimum_required(VERSION 3.10)
project(UserApps C)

# 设置 RISC-V 交叉编译工具链
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_CXX_COMPILER riscv64-unknown-elf-g++)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)

# ✅ 使用标准 C 库
set(CMAKE_C_FLAGS "-Wall -Werror -g -Og -std=gnu99 -static")


# ✅ 设定 ELF 文件的输出目录（将用户程序放在 build/bin）
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
# ✅ 设定 objdump 和 readelf 文件的输出目录（解析文件放 `./build/bin/debug/`）
set(DEBUG_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/debug)
# 生成 app_exec 可执行文件
add_executable(app_exec app_exec.c)

# ✅ 让用户程序链接标准 C 库
target_link_libraries(app_exec c)

# ✅ 额外的静态链接选项
set_target_properties(app_exec PROPERTIES LINK_FLAGS "-static --entry=_start")

# ✅ 生成 objdump 和 ELF 解析文件
add_custom_command(
    TARGET app_exec
    POST_BUILD
    COMMAND mkdir -p ${DEBUG_OUTPUT_DIRECTORY}  # 确保 debug 目录存在
    COMMAND riscv64-unknown-elf-objdump -d ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/app_exec > ${DEBUG_OUTPUT_DIRECTORY}/app_exec.dump
    COMMAND riscv64-unknown-elf-readelf -a ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/app_exec > ${DEBUG_OUTPUT_DIRECTORY}/app_exec.elfinfo
    COMMENT "Generating objdump and ELF analysis for app_exec in ${DEBUG_OUTPUT_DIRECTORY}"
)
