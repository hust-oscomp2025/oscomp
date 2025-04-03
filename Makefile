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
  -nographic \
  -bios default \
  -kernel build/bin/riscv-pke \
	-s -S

gdb-client:
	riscv64-unknown-elf-gdb -ex "target remote localhost:1234" \
	-ex "b s_start" \
	build/bin/riscv-pke -q