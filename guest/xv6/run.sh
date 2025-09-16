#!/bin/bash

HELP="Usage:
  ./run.sh gdb      加载kernel.img运行, 并通过gdb调试
  ./run.sh gdb elf  加载kernel运行, 并通过gdb调试
  ./run.sh elf      加载kernel运行
  ./run.sh          加载kernel.img运行

说明:
  - gdb: 启用调试模式
  - elf: 使用ELF格式的内核文件
  - 默认: 使用二进制格式的内核文件 (kernel.img)"


QEMU=/home/hbk/workspace/tmp/qemu-7.0.0/install/bin/qemu-system-aarch64

KERNEL_IMG=kernel/kernel.img
KERNEL=kernel/kernel


if [[ $1 = "gdb" ]];then
    if [[ $2 = "elf" ]];then
        $QEMU -cpu cortex-a72 \
              -machine virt,gic-version=3 \
              -kernel $KERNEL \
              -m 128M \
              -smp 4 \
              -nographic \
              -drive file=fs.img,if=none,format=raw,id=x0 \
              -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
              -S -gdb tcp::7758
    else
        $QEMU -cpu cortex-a72 \
              -machine virt,gic-version=3 \
              -kernel $KERNEL_IMG \
              -m 128M \
              -smp 4 \
              -nographic \
              -drive file=fs.img,if=none,format=raw,id=x0 \
              -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
              -S -gdb tcp::7758
    fi
elif [[ $1 = "elf" ]];then
    $QEMU -cpu cortex-a72 \
          -machine virt,gic-version=3 \
          -kernel $KERNEL \
          -m 128M \
          -smp 4 \
          -nographic \
          -drive file=fs.img,if=none,format=raw,id=x0 \
          -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
elif [[ $1 = "--help" || $1 = "-h" ]];then
    echo "$HELP"
else
    # $QEMU -cpu cortex-a72 \
    #       -machine virt,gic-version=3 \
    #       -kernel $KERNEL_IMG \
    #       -m 128M \
    #       -smp 4 \
    #       -nographic \
    #       -drive file=fs.img,if=none,format=raw,id=x0 \
    #       -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

    $QEMU -cpu cortex-a72 \
          -machine virt,gic-version=3 \
          -kernel $KERNEL_IMG \
          -m 128M \
          -smp 4 \
          -nographic
fi