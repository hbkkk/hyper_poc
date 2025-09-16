#ifndef RAMDISK_H
#define RAMDISK_H

#include "types.h"

#define FSIMG_SIZE  1000
#define BLOCK_SIZE  1024

void ramdisk_init(void);

int ramdisk_rw(u64 blk_num, u64 bufaddr, u16 is_write);

#endif