# RISC-V GDB命令速查表 (VS Code Debug Console)

> 注意：在VS Code Debug Console中，所有GDB命令都需要加上`-exec`前缀。

## 基本控制命令
| 命令 | 说明 |
|------|------|
| `-exec continue` | 继续执行到下一个断点 |
| `-exec next` | 单步执行（跳过函数） |
| `-exec step` | 单步执行（进入函数） |
| `-exec finish` | 运行到当前函数返回 |
| `-exec until 行号` | 运行到指定行 |
| `-exec interrupt` | 中断程序执行 |

## 查看寄存器
| 命令 | 说明 |
|------|------|
| `-exec info registers` | 查看所有通用寄存器 |
| `-exec info registers pc sp ra` | 查看特定寄存器 |
| `-exec info all-registers` | 查看所有寄存器（包括系统寄存器） |
| `-exec info registers csr` | 查看RISC-V控制状态寄存器 |
| `-exec p/x $pc` | 以十六进制打印PC寄存器值 |
| `-exec p $a0` | 打印a0寄存器值 |

## 断点管理
| 命令 | 说明 |
|------|------|
| `-exec break 函数名` | 在函数处设置断点 |
| `-exec break 文件名:行号` | 在源文件特定行设置断点 |
| `-exec break *0x80000000` | 在内存地址处设置断点 |
| `-exec info breakpoints` | 列出所有断点 |
| `-exec delete 断点号` | 删除指定断点 |
| `-exec disable 断点号` | 禁用断点 |
| `-exec enable 断点号` | 启用断点 |
| `-exec clear 行号` | 清除特定行的断点 |

## 内存检查
| 命令 | 说明 |
|------|------|
| `-exec x/10xw 地址` | 以十六进制显示10个字 |
| `-exec x/s 地址` | 显示内存中的字符串 |
| `-exec x/i $pc` | 反汇编当前PC处的指令 |
| `-exec x/10i 地址` | 反汇编10条指令 |
| `-exec dump binary memory file.bin 起始地址 结束地址` | 将内存转储到文件 |

## 栈帧操作
| 命令 | 说明 |
|------|------|
| `-exec backtrace` | 显示调用堆栈 |
| `-exec info frame` | 显示当前栈帧信息 |
| `-exec up` | 移动到上一层栈帧 |
| `-exec down` | 移动到下一层栈帧 |
| `-exec frame 帧号` | 切换到指定栈帧 |

## 变量和符号
| 命令 | 说明 |
|------|------|
| `-exec info locals` | 显示局部变量 |
| `-exec print 变量名` | 打印变量值 |
| `-exec print/x 变量名` | 以十六进制打印变量 |
| `-exec set var 变量名=值` | 设置变量值 |
| `-exec info variables` | 显示所有全局和静态变量 |
| `-exec whatis 变量名` | 显示变量类型 |
| `-exec ptype 变量名或类型名` | 显示详细类型信息 |

## RISC-V特定命令
| 命令 | 说明 |
|------|------|
| `-exec set riscv use-compressed-breakpoints on` | 启用压缩指令断点 |
| `-exec set riscv verbose-disassembler on` | 详细反汇编 |
| `-exec set debug-file-directory 路径` | 设置调试文件目录 |

## 其他有用命令
| 命令 | 说明 |
|------|------|
| `-exec show logging` | 显示日志状态 |
| `-exec set logging on` | 启用日志 |
| `-exec disassemble` | 反汇编当前函数 |
| `-exec info threads` | 显示所有线程 |
| `-exec thread 线程号` | 切换到指定线程 |
| `-exec set print array on` | 完整打印数组 |
| `-exec set print pretty on` | 格式化打印结构体 |
| `-exec help` | 显示GDB帮助 |

## 常用打印格式
| 格式修饰符 | 说明 |
|------|------|
| `/x` | 十六进制 |
| `/d` | 十进制 |
| `/u` | 无符号十进制 |
| `/o` | 八进制 |
| `/t` | 二进制 |
| `/a` | 地址 |
| `/c` | 字符 |
| `/f` | 浮点数 |
| `/s` | 字符串 |
| `/i` | 机器指令 |

## 内存检查格式
格式：`x/nfu 地址`
- `n`: 要显示的单元数量
- `f`: 显示格式（同上面的打印格式）
- `u`: 单元大小
  - `b`: 字节(8位)
  - `h`: 半字(16位)
  - `w`: 字(32位)
  - `g`: 巨字(64位)

例如：`-exec x/10xw 0x80000000` 显示10个32位字，十六进制格式