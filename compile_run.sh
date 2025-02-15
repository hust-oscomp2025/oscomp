#!/bin/bash

make clean
make
spike ./obj/riscv-pke ./obj/app_long_loop
# 修改./obj/{binary file}即可运行其他脚本