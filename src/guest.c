#include "guest.h"

#define HYPER_VM_PACKED     1

extern char _binary_guest_xv6_start[];
extern char _binary_guest_xv6_size[];   // mem size of xv6 load segment, include .bss section
extern char _binary_guest_xv6_end[];    // end-start: file size of xv6 load segment, exclude .bss section

extern char _binary_guest_xv6_fs_img_start[];
extern char _binary_guest_xv6_fs_img_size[];
extern char _binary_guest_xv6_fs_img_end[];

struct guest guest_xv6[] = {
    {
        .name = "xv6",
#if HYPER_VM_PACKED
        .start = (u64)_binary_guest_xv6_start,
        .end = (u64)_binary_guest_xv6_end,
        .size = (u64)_binary_guest_xv6_size,
#else
        .start = (u64)0x4001000,
        // .start = (u64)0x4003000,
        // .start = (u64)0x80000000,
        .size = (u64)0x4000,
#endif
    },
    {
        .name = NULL,
        .start = 0,
        .size = 0,
    },
    {
        .name = NULL,
        .start = 0,
        .size = 0,
    },
};