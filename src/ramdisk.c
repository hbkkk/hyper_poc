#include "ramdisk.h"
#include "lib.h"
#include "debug.h"

static u64 ramdisk_start;
static u64 ramdisk_size;

extern char _binary_guest_xv6_fs_img_start[];
extern char _binary_guest_xv6_fs_img_size[];
extern char _binary_guest_xv6_fs_img_end[];

void ramdisk_init(void)
{
    ramdisk_start = (u64)_binary_guest_xv6_fs_img_start;
    ramdisk_size = (u64)_binary_guest_xv6_fs_img_size;
}

int ramdisk_rw(u64 blk_num, u64 buf_addr, u16 is_write)
{
    /* TODO:
     * Need lock or not ??? (upper app ensure the coherence)
     *
     * Maybe unnecessary to lock, because virtio_blk_process_desc has already
     * been protected by g_vq.virtq_lock in virtio_blk_req_handler.
     */
    if (blk_num >= FSIMG_SIZE) {
        LOG_ERR("[ramdisk_rw] ERROR !!! invalid blockno(%d)\n", blk_num);
        return -1;
    }

    // TODO: check whether buf_addr is valid ?

    u8 *disk_addr = (u8*)(ramdisk_start + blk_num * BLOCK_SIZE);
    if (is_write) {
        memmove(disk_addr, (void*)buf_addr, BLOCK_SIZE);
    } else {
        memmove((void*)buf_addr, disk_addr, BLOCK_SIZE);
    }

    return 0;
}