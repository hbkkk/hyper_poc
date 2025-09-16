PREFIX = aarch64-linux-gnu-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
OBJCOPY = $(PREFIX)objcopy
TARGET = hyper
CPU = cortex-a72

DEBUG_MODE ?= 0

CFLAGS = -Wall -O0 -g -ffreestanding -nostdinc -nostdlib -nostartfiles -mcpu=$(CPU)
CFLAGS += -I ./include/
CFLAGS += -DRAM_SIZE=0x10000000
CFLAGS += -DSMP_NUM=4
CFLAGS += -DDEBUG_MODE=$(DEBUG_MODE)

LDFLAGS = -nostdlib

OBJS = src/boot.o src/vector.o src/init.o src/lib.o src/uart.o src/printf.o src/gic_v3.o \
       src/trap.o src/sysreg.o src/timer.o src/vcpu.o src/vm.o src/mmu.o src/page_alloc.o \
	   src/vgic.o src/guest.o src/mmio.o src/sp_init.o src/psci.o src/virtio_dev.o \
	   src/ramdisk.o src/calltrace.o

all: hyper

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

guest/xv6/kernel/xv6.img: guest/xv6/Makefile
	make -C guest/xv6

guest/xv6/fsimg.o: guest/xv6/fs.img
	$(LD) -r -b binary $< -o $@

.xv6_size: guest/xv6/kernel/xv6.img ./cal_xv6size.sh
	@./cal_xv6size.sh

$(TARGET): $(OBJS) linker.ld guest/xv6/kernel/xv6.img guest/xv6/fsimg.o .xv6_size
	@echo "Checking XV6_SIZE=$(shell cat .xv6_size)"
	$(LD) -r -b binary guest/xv6/kernel/xv6.img -o xv6.o
	$(OBJCOPY) --redefine-sym _binary_guest_xv6_kernel_xv6_img_start=_binary_guest_xv6_start \
			   --redefine-sym _binary_guest_xv6_kernel_xv6_img_end=_binary_guest_xv6_end \
			   --redefine-sym _binary_guest_xv6_kernel_xv6_img_size=_binary_guest_xv6_size xv6.o
	$(LD) $(LDFLAGS) -T linker.ld --defsym=_binary_guest_xv6_size=$(shell cat .xv6_size) -o $@ $(OBJS) \
	        xv6.o guest/xv6/fsimg.o

clean:
	make -C guest/xv6 clean
	$(RM) $(OBJS) $(TARGET) xv6.o .xv6_size

.PHONY:  clean all