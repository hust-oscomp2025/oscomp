cd ./vendor/busybox

# Create initial default config
make defconfig

# Edit the configuration for RISC-V
sed -i 's/CONFIG_CROSS_COMPILER_PREFIX=""/CONFIG_CROSS_COMPILER_PREFIX="riscv64-linux-gnu-"/' .config
sed -i 's/# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config
sed -i '/CONFIG_TC=/d' .config 
echo "CONFIG_TC=n" >> .config
sed -i 's/.*CONFIG_SHA1_HWACCEL.*/# CONFIG_SHA1_HWACCEL is not set/' .config
sed -i 's/.*CONFIG_SHA256_HWACCEL.*/# CONFIG_SHA256_HWACCEL is not set/' .config

# 禁用流量控制模块
# Build BusyBox
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- clean
make ARCH=riscv  CROSS_COMPILE=riscv64-linux-gnu- -j$(nproc)
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- CONFIG_PREFIX=/root/workspace/oscomp-release/hostfs install