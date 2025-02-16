#!/bin/bash

make clean
> ./logs/make.log
make >> ./logs/make.log

# 修改./obj/{binary file}即可运行其他脚本