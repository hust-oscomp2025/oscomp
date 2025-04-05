MUSL_BUILD_DIR = $(shell pwd)/build-musl
MUSL_LIB = $(MUSL_BUILD_DIR)/lib/libc.a
MUSL_SCRIPT = script/build-musl.sh
MUSL_SOURCE = vendor/musl

# 目录设置
PROJECT_ROOT := $(shell pwd)
BUILD_DIR := $(PROJECT_ROOT)/build
KERNEL_DIR := $(PROJECT_ROOT)/kernel
USER_DIR := $(PROJECT_ROOT)/user
VENDOR_DIR := $(PROJECT_ROOT)/vendor
SCRIPT_DIR := $(PROJECT_ROOT)/script


# MUSL 构建目标
.PHONY: musl musl-clean build kernel busybox run gdb gdb-client clean rebuild lwext4 example-with-musl cmake-build

# CMake构建
cmake-build:
	@echo "使用CMake构建项目..."
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake .. && make


kernel:
	@bash script/build.sh 
build:
	@bash script/build.sh 
busybox:
	@bash script/make_busybox.sh
run: 
	qemu-system-riscv64 \
  -machine virt \
  -nographic \
  -m 64M \
  -bios default \
  -kernel build/bin/riscv-pke 
#  >> /logs.txt
#  -append "app_exec"

gdb:
	qemu-system-riscv64 \
  -machine virt \
  -m 64M \
  -nographic \
  -bios default \
  -kernel build/bin/riscv-pke \
	-s -S

gdb-client:
	riscv64-unknown-elf-gdb -x gdbinit.txt build/bin/riscv-pke -q
clean:
	rm -rf build
rebuild:
	rm -rf build
	@bash script/build.sh 
	@bash script/make_busybox.sh
#  -bios /root/workspace/oscomp-dev/vendor/opensbi/build/platform/generic/firmware/fw_jump.bin 
lwext4:
	@bash script/build-lwext4.sh

musl: $(MUSL_LIB)
$(MUSL_LIB): $(MUSL_SCRIPT) $(MUSL_SOURCE)
	@echo "构建 MUSL riscv64 静态库..."
	@chmod +x $(MUSL_SCRIPT)
	@$(MUSL_SCRIPT)
	@echo "MUSL 构建完成"

musl-clean:
	@echo "清理 MUSL 构建..."
	@rm -rf build-musl
	@echo "MUSL 清理完成"

# 使用MUSL的示例目标（可选）
.PHONY: example-with-musl

example-with-musl: $(MUSL_LIB)
	@echo "使用MUSL静态库编译示例程序..."
	riscv64-unknown-elf-gcc -static -o example example.c \
		-L$(MUSL_BUILD_DIR)/lib \
		-I$(MUSL_SOURCE)/include \
		-lc