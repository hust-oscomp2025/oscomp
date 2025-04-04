run: 
	qemu-system-riscv64 \
  -machine virt \
  -nographic \
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


#  -bios /root/workspace/oscomp-dev/vendor/opensbi/build/platform/generic/firmware/fw_jump.bin 