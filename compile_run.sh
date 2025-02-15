#!/bin/bash

make clean
make
spike ./obj/riscv-pke ./obj/app_print_backtrace
# 修改./obj/{binary file}即可运行其他脚本