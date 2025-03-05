cmake_minimum_required(VERSION 3.10)
project(riscv-pke C ASM)
# ✅ 添加用户态程序的编译（调用 user/ 目录的 CMakeLists.txt）
add_subdirectory(user)
add_subdirectory(kernel)
# Set cross-compiler
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_C_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_ASM_COMPILER riscv64-unknown-elf-gcc)
set(CMAKE_AR riscv64-unknown-elf-ar)
set(CMAKE_RANLIB riscv64-unknown-elf-ranlib)

# Compiler flags
set(CMAKE_C_FLAGS "-Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE -fno-omit-frame-pointer")

# Apply the same flags to ASM
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS}")

# Set output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Create hostfs_root/bin directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/hostfs_root/bin)

# Include directories
include_directories(.)



# Build util library
file(GLOB UTIL_SOURCES "util/*.c")
add_library(util STATIC ${UTIL_SOURCES})
target_include_directories(util PRIVATE ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/util {CMAKE_SOURCE_DIR}/spike_interface)

# Build spike_interface library
file(GLOB SPIKE_INF_SOURCES "spike_interface/*.c")
add_library(spike_interface STATIC ${SPIKE_INF_SOURCES})

# 确保 spike_interface 和 util 先编译完成
add_dependencies(spike_interface util)
target_link_libraries(spike_interface util)
target_include_directories(spike_interface PRIVATE 
	${CMAKE_SOURCE_DIR}/include 
	${CMAKE_SOURCE_DIR}/util
	${CMAKE_SOURCE_DIR}
)


# Custom targets for convenience
add_custom_target(run
    COMMAND spike ${CMAKE_BINARY_DIR}/bin/riscv-pke /bin/app_exec
    DEPENDS riscv-pke app_exec
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running HUST PKE with spike"
)

add_custom_target(gdb
    COMMAND spike --rbb-port=9824 -H ${CMAKE_BINARY_DIR}/bin/riscv-pke /bin/app_exec &
    COMMAND sleep 1
    COMMAND openocd -f ${CMAKE_CURRENT_SOURCE_DIR}/.spike.cfg &
    COMMAND sleep 1
    COMMAND riscv64-unknown-elf-gdb -command=${CMAKE_CURRENT_SOURCE_DIR}/.gdbinit
    DEPENDS riscv-pke app_exec app_ls
    COMMENT "Starting GDB debugging session"
)

add_custom_target(gdb_clean
    COMMAND kill -9 `lsof -i:9824 -t` || true
    COMMAND kill -9 `lsof -i:3333 -t` || true
    COMMAND sleep 1
    COMMENT "Cleaning up GDB session"
)

add_custom_target(objdump
    COMMAND riscv64-unknown-elf-objdump -d ${CMAKE_BINARY_DIR}/bin/riscv-pke > ${CMAKE_BINARY_DIR}/kernel_dump
    COMMAND riscv64-unknown-elf-objdump -d ${CMAKE_BINARY_DIR}/hostfs_root/bin/app_exec > ${CMAKE_BINARY_DIR}/user_dump
    DEPENDS riscv-pke app_exec
    COMMENT "Generating objdump files"
)

add_custom_target(cscope
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.c" > cscope.files
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.h" >> cscope.files
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.S" >> cscope.files
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.lds" >> cscope.files
    COMMAND cscope -bqk
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating cscope database"
)

add_custom_target(format
    COMMAND python ${CMAKE_CURRENT_SOURCE_DIR}/format.py ${CMAKE_CURRENT_SOURCE_DIR}/
    COMMENT "Formatting code"
)