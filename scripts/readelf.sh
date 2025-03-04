#!/bin/bash
> ./logs/readelf.log
readelf -a ./obj/app_print_backtrace >> ./logs/readelf.log
> ./logs/objdump.log
riscv64-unknown-elf-objdump -d obj/app_print_backtrace >> ./logs/objdump.log