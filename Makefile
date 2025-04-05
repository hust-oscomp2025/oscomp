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