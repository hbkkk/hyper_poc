#ifndef GUEST_H
#define GUEST_H

#include "types.h"

#define GUEST_IMAGE     0
#define GUEST_FDT       1
#define GUEST_INITRD     2

struct guest {
    char    *name;

    /* guest os/initrd/rootfs 被加载到真正物理内存的地址,
     * hypervisor需要从该地址中读取guestos image内容并建立二级页表映射) */
    u64     start;

    /* guest os/initrd/rootfs 的大小
     * 对于guest image而言, 是guest image LOAD segment 的 mem_size, include .bss */
    u64     size;

    /* guest img LOAD类型segment中最后内容的地址,
     * end-start = guest img LOAD segment's file size */
    u64     end;
};

#endif