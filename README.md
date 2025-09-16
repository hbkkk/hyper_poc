# hyper_poc
Armv8 Type-1 Hypervisor 


# Overview
This project is based on the AArch64 architecture, including a Hypervisor and  Guest OS(xv6). 


# Supported feature
- memory virtualization (stage2 translation)
- cpu virtualization
- gicd/gicr virtualization
- virtual interrupt injection
- uart pass through
- virtio blk backend device (implemented in hyper)
- wfi/wfe emulation
- VM SPM support(vpsci emulation)
- EL2 stack canary protection
- hypervisor and VM's calltrace


# Prerequisites
Ensure the following tools are installed:
- Cross-compilation toolchain: aarch64-linux-gnu-* (gcc, ld, objcopy)
- QEMU emulator: qemu-system-aarch64
- Multi-architecture GDB: gdb-multiarch
- Make tool


# Usage
- ./build.sh
- ./run.sh


# Debug hypervisor
- ./run.sh gdb
- ./debug.sh


# Example
```bash
$ ./run.sh 
=====================  BASIC memeroy information  =====================
 - Total ram size : 0x10000000(256MB)
 - RAM_PAGE_NUM   : 0x10000
 - BITMAP_SIZE    : 0x400
 - ram start addr : 0x40122000
 - txt_start addr : 0x40000000
=======================================================================
enter vm(xv6)'s vcpu 0
enter vm(xv6)'s vcpu 1
enter vm(xv6)'s vcpu 3
enter vm(xv6)'s vcpu 2

xv6 kernel is booting

lines 7
hart 1 starting
hart 2 starting
hart 3 starting
init: starting sh
$ 
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2226
cat            2 3 14744
echo           2 4 14192
forktest       2 5 8200
grep           2 6 16048
init           2 7 14712
kill           2 8 14136
ln             2 9 14144
ls             2 10 15800
mkdir          2 11 14232
rm             2 12 14216
sh             2 13 22488
stressfs       2 14 14576
usertests      2 15 68744
grind          2 16 20304
wc             2 17 15040
zombie         2 18 13872
console        3 19 0
$ 
$