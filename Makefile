# we assume that the utilities from RISC-V cross-compiler (i.e., riscv64-unknown-elf-gcc and etc.)
# are in your system PATH. To check if your environment satisfies this requirement, simple use 
# `which` command as follows:
# $ which riscv64-unknown-elf-gcc
# if you have an output path, your environment satisfy our requirement.

# ---------------------	macros --------------------------
CROSS_PREFIX 	:= riscv64-unknown-elf-
CC 				:= $(CROSS_PREFIX)gcc
AR 				:= $(CROSS_PREFIX)ar
RANLIB        	:= $(CROSS_PREFIX)ranlib

SRC_DIR        	:= .
OBJ_DIR 		:= obj
SPROJS_INCLUDE 	:= -I.  

ifneq (,)
  march := -march=
  is_32bit := $(findstring 32,$(march))
  mabi := -mabi=$(if $(is_32bit),ilp32,lp64)
endif

CFLAGS        := -Wall -Werror -gdwarf-3 -fno-builtin -nostdlib -D__NO_INLINE__ -mcmodel=medany -g -Og -std=gnu99 -Wno-unused -Wno-attributes -fno-delete-null-pointer-checks -fno-PIE $(march) -fno-omit-frame-pointer
COMPILE       	:= $(CC) -MMD -MP $(CFLAGS) $(SPROJS_INCLUDE)

#---------------------	utils -----------------------
UTIL_CPPS 	:= util/*.c

UTIL_CPPS  := $(wildcard $(UTIL_CPPS))
UTIL_OBJS  :=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(UTIL_CPPS)))


UTIL_LIB   := $(OBJ_DIR)/util.a

#---------------------	kernel -----------------------
KERNEL_LDS  	:= kernel/kernel.lds
KERNEL_CPPS 	:= \
	kernel/*.c \
	kernel/machine/*.c \
	kernel/util/*.c

KERNEL_ASMS 	:= \
	kernel/*.S \
	kernel/machine/*.S \
	kernel/util/*.S

KERNEL_CPPS  	:= $(wildcard $(KERNEL_CPPS))
KERNEL_ASMS  	:= $(wildcard $(KERNEL_ASMS))
KERNEL_OBJS  	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(KERNEL_CPPS)))
KERNEL_OBJS  	+=  $(addprefix $(OBJ_DIR)/, $(patsubst %.S,%.o,$(KERNEL_ASMS)))

KERNEL_TARGET = $(OBJ_DIR)/riscv-pke


#---------------------	spike interface library -----------------------
SPIKE_INF_CPPS 	:= spike_interface/*.c

SPIKE_INF_CPPS  := $(wildcard $(SPIKE_INF_CPPS))
SPIKE_INF_OBJS 	:=  $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(SPIKE_INF_CPPS)))


SPIKE_INF_LIB   := $(OBJ_DIR)/spike_interface.a


#---------------------	user   -----------------------
USER_LDS0  := user/user0.lds
USER_LDS1  := user/user1.lds

USER_CPP0 		:= user/app0.c user/user_lib.c
USER_CPP1 		:= user/app1.c user/user_lib.c

USER_OBJ0  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPP0)))
USER_OBJ1  		:= $(addprefix $(OBJ_DIR)/, $(patsubst %.c,%.o,$(USER_CPP1)))

USER_TARGET0 	:= $(OBJ_DIR)/app0
USER_TARGET1 	:= $(OBJ_DIR)/app1
#------------------------targets------------------------

$(OBJ_DIR):
	@-mkdir -p $(OBJ_DIR)	
	@-mkdir -p $(dir $(UTIL_OBJS))
	@-mkdir -p $(dir $(SPIKE_INF_OBJS))
	@-mkdir -p $(dir $(KERNEL_OBJS))
	@-mkdir -p $(dir $(USER_OBJ0))
	@-mkdir -p $(dir $(USER_OBJ1))

$(OBJ_DIR)/%.o : %.c
	@echo "compiling" $< >> ./logs/makefile.log 2>&1
	@> ./logs/compiler.log
	@$(COMPILE) -c $< -o $@ >> ./logs/compiler.log 2>&1

$(OBJ_DIR)/%.o : %.S
	@echo "compiling" $< >> ./logs/makefile.log 2>&1
	@$(COMPILE) -c $< -o $@ >> ./logs/compiler.log 2>&1

$(UTIL_LIB): $(OBJ_DIR) $(UTIL_OBJS)
	@echo "linking " $@	...	 >> ./logs/makefile.log 2>&1
	@$(AR) -rcs $@ $(UTIL_OBJS)  >> ./logs/compiler.log 2>&1
	@echo "Util lib has been build into" \"$@\" >> ./logs/makefile.log 2>&1
	
$(SPIKE_INF_LIB): $(OBJ_DIR) $(UTIL_OBJS) $(SPIKE_INF_OBJS)
	@echo "linking " $@	...	 >> ./logs/makefile.log 2>&1
	@$(AR) -rcs $@ $(SPIKE_INF_OBJS) $(UTIL_OBJS) >> ./logs/compiler.log 2>&1
	@echo "Spike lib has been build into" \"$@\" >> ./logs/makefile.log 2>&1

$(KERNEL_TARGET): $(OBJ_DIR) $(UTIL_LIB) $(SPIKE_INF_LIB) $(KERNEL_OBJS) $(KERNEL_LDS)
	@echo "linking" $@ ... >> ./logs/makefile.log 2>&1
	@$(COMPILE) $(KERNEL_OBJS) $(UTIL_LIB) $(SPIKE_INF_LIB) -o $@ -T $(KERNEL_LDS) >> ./logs/compiler.log 2>&1
	@echo "PKE core has been built into" \"$@\" >> ./logs/makefile.log 2>&1

$(USER_TARGET0): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJ0) $(USER_LDS0)
	@echo "linking" $@	...	
	@$(COMPILE) $(USER_OBJ0) $(UTIL_LIB) -o $@ -T $(USER_LDS0)
	@echo "User app has been built into" \"$@\"
	
$(USER_TARGET1): $(OBJ_DIR) $(UTIL_LIB) $(USER_OBJ1) $(USER_LDS1)
	@echo "linking" $@	...	
	@$(COMPILE) $(USER_OBJ1) $(UTIL_LIB) -o $@ -T $(USER_LDS1)
	@echo "User app has been built into" \"$@\"

-include $(wildcard $(OBJ_DIR)/*/*.d)
-include $(wildcard $(OBJ_DIR)/*/*/*.d)

.DEFAULT_GOAL := $(all)

all: $(KERNEL_TARGET) $(USER_TARGET0) $(USER_TARGET1)
.PHONY:all

run: $(KERNEL_TARGET) $(USER_TARGET0) $(USER_TARGET1)
	@echo "********************HUST PKE********************"
	spike -p2 $(KERNEL_TARGET) $(USER_TARGET0) $(USER_TARGET1)

# need openocd!
gdb:$(KERNEL_TARGET) $(USER_TARGET0) $(USER_TARGET1)
	spike --rbb-port=9824 -H -p2 $(KERNEL_TARGET) $(USER_TARGET0) $(USER_TARGET1) &
	@sleep 1
	openocd -f ./.spike.cfg &
	@sleep 1
	riscv64-unknown-elf-gdb -command=./.gdbinit

# clean gdb. need openocd!
gdb_clean:
	@-kill -9 $$(lsof -i:9824 -t)
	@-kill -9 $$(lsof -i:3333 -t)
	@sleep 1

objdump:
	riscv64-unknown-elf-objdump -d $(KERNEL_TARGET) > $(OBJ_DIR)/kernel_dump
	riscv64-unknown-elf-objdump -d $(USER_TARGET0) > $(OBJ_DIR)/app0_dump
	riscv64-unknown-elf-objdump -d $(USER_TARGET1) > $(OBJ_DIR)/app1_dump

cscope:
	find ./ -name "*.c" > cscope.files
	find ./ -name "*.h" >> cscope.files
	find ./ -name "*.S" >> cscope.files
	find ./ -name "*.lds" >> cscope.files
	cscope -bqk

format:
	@python ./format.py ./

clean:
	rm -fr ${OBJ_DIR}

print_user_target:
	@echo $(USER_TARGET)
