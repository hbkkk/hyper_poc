#ifndef VIRTIO_H
#define VIRTIO_H

#include "types.h"
#include "spinlock.h"

struct vm;

#define VIRTIO_MMIO_MAGIC_VALUE		    0x000 // 0x74726976, read-only
#define VIRTIO_MMIO_VERSION		        0x004 // version; 1 is legacy, read-only
#define VIRTIO_MMIO_DEVICE_ID		    0x008 // device type; 1 is net, 2 is disk, read-only
#define VIRTIO_MMIO_VENDOR_ID		    0x00c // 0x554d4551, read-only
#define VIRTIO_MMIO_DEVICE_FEATURES	    0x010 // Flags representing features the device supports, read-only
#define VIRTIO_MMIO_DRIVER_FEATURES	    0x020 // write-only
#define VIRTIO_MMIO_GUEST_PAGE_SIZE	    0x028 // page size for PFN, write-only
#define VIRTIO_MMIO_QUEUE_SEL		    0x030 // select queue, write-only
#define VIRTIO_MMIO_QUEUE_NUM_MAX	    0x034 // max size of current queue, read-only
#define VIRTIO_MMIO_QUEUE_NUM		    0x038 // size of current queue, write-only
#define VIRTIO_MMIO_QUEUE_ALIGN		    0x03c // used ring alignment, write-only
#define VIRTIO_MMIO_QUEUE_PFN		    0x040 // physical page number for queue, read/write
#define VIRTIO_MMIO_QUEUE_READY		    0x044 // ready bit, read/write
#define VIRTIO_MMIO_QUEUE_NOTIFY	    0x050 // Writing a queue index to this register notifies the device that
                                              // there are new buffers to process in the queue, write-only
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060 // read-only
#define VIRTIO_MMIO_INTERRUPT_ACK	    0x064 // notifies the device that events causing the interrupt have been handled, write-only
#define VIRTIO_MMIO_STATUS		        0x070 // read/write

#define VIRTIO_CONFIG_S_ACKNOWLEDGE	1
#define VIRTIO_CONFIG_S_DRIVER		2
#define VIRTIO_CONFIG_S_DRIVER_OK	4
#define VIRTIO_CONFIG_S_FEATURES_OK	8

// device feature bits
#define VIRTIO_BLK_F_RO              5	/* Disk is read-only */
#define VIRTIO_BLK_F_SCSI            7	/* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE     11	/* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ             12	/* support more than one vq */
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// this many virtio descriptors.
// must be a power of two.
#define NUM 8

#define VIRTIO0_IRQ  48

struct virtq_desc {
    u64 addr;
    u32 len;
    u16 flags;
    u16 next;
};

#define VRING_DESC_F_NEXT  1 // chained with another descriptor
#define VRING_DESC_F_WRITE 2 // device writes (vs read)

struct virtq_avail {
    u16 flags; // always zero
    u16 idx;   // driver will write ring[idx] next
    u16 ring[NUM]; // descriptor numbers of chain heads
    u16 unused;
};

struct virtq_used_elem {
    u32 id;   // index of start of completed descriptor chain
    u32 len;
};

struct virtq_used {
    u16 flags; // always zero
    u16 idx;   // device increments when it adds a ring[] entry
    struct virtq_used_elem ring[NUM];
};

#define VIRTIO_BLK_T_IN  0 // read the disk
#define VIRTIO_BLK_T_OUT 1 // write the disk
#define VIRTIO_BLK_T_FLUSH 4

// the format of the first descriptor in a disk request.
// to be followed by two more descriptors containing
// the block, and a one-byte status.
struct virtio_blk_req {
    u32 type; // VIRTIO_BLK_T_IN or ..._OUT
    u32 reserved;
    u64 sector;
};

struct virt_queue {
    spinlock_t          virtq_lock;
    u64                 vring_num;
    u64                 vring_ipa;
    u64                 vring_pa;

    /* avail_idx will always be incremented, not % vring_num.
     * It's where we assume the next request index is at. */
    u16                 avail_idx;      
    
    /* point to vring's PA, where are samed with VM's vring */
    struct virtq_desc   *desc;
    struct virtq_avail  *avail;
    struct virtq_used   *used;
};

void virtio_mmio_init(struct vm *vm);

#endif