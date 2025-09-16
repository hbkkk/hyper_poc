#include "virtio.h"
#include "memmap.h"
#include "mmio.h"
#include "vcpu.h"
#include "ramdisk.h"
#include "mmu.h"
#include "vm.h"
#include "debug.h"

#define DESC_IDX_BLK_REQ        0
#define DESC_IDX_BUFFER         1
#define DESC_IDX_REQ_STATUS     2

static u64 guest_pagesz = 0;
static u64 g_queue_sel = 0;

struct virt_queue g_vq = {0};

static u64 virtio_guest_to_host(u64 ipa)
{
    struct vcpu *vcpu = cur_vcpu();
    u64 *pgt = vcpu->vm->stage2_pt;

    return ipa2pa(pgt, ipa);
}

static void vq_ring_init(u64 ipa)
{
    spinlock_init(&g_vq.virtq_lock);

    g_vq.vring_ipa = ipa;
    g_vq.vring_pa = virtio_guest_to_host(ipa);
    LOG_INFO("g_vq.vring_ipa=%p, g_vq.vring_pa=%p\n", g_vq.vring_ipa, g_vq.vring_pa);

    g_vq.avail = (struct virtq_avail *)(g_vq.vring_pa + g_vq.vring_num * sizeof(struct virtq_desc));
    g_vq.desc = (struct virtq_desc *)g_vq.vring_pa;
    g_vq.used = (struct virtq_used *) (g_vq.vring_pa + PAGE_SIZE);
}

/* Notify FE that virtio request has been processed. */
static void virtio_signal_vq(void)
{
    gic_set_pending_irq(VIRTIO0_IRQ);
}

static void vqueue_set_used_elem(u16 desc_idx, u16 len)
{
    g_vq.used->ring[g_vq.used->idx % g_vq.vring_num].id = (u32)desc_idx;
    g_vq.used->ring[g_vq.used->idx % g_vq.vring_num].len = (u32)len;

    __sync_synchronize();
    g_vq.used->idx += 1;
    __sync_synchronize();
}

static bool virtq_available()
{
    return g_vq.avail_idx != g_vq.avail->idx;
}

static inline bool virt_desc_test_flag(struct virtq_desc *desc, u16 flags)
{
    return !!(desc->flags & flags);
}

static u16 next_desc(struct virtq_desc *desc, u16 i, u16 max)
{
    u16 next = 0;
    if (!virt_desc_test_flag(&desc[i], VRING_DESC_F_NEXT)) {
        return max;
    }
    next = desc[i].next;

    return (next < max) ? next : max;
}

static u16 virtio_blk_process_desc(u16 desc_idx)
{
    u16 idx = 0;
    u16 max = 0;
    u16 desc_len = 0;
    u16 is_write = 0;
    u64 buf_addr = 0;
    u64 blk_num = 0;
    int ret = -1;
    struct virtio_blk_req *virt_blk_req = NULL;
    struct virtq_desc desc[g_vq.vring_num];

    idx = desc_idx;
    max = g_vq.vring_num;
    do {
        // LOG_INFO("[virtio_blk_process_desc]: idx = %d\n", idx);
        desc[desc_len] = g_vq.desc[idx];
        ++desc_len;
        idx = next_desc(g_vq.desc, idx, max);
    } while (idx != max);
    
    // LOG_INFO("[virtio_blk_process_desc]: desc_len = %d\n", desc_len);
    
    if (desc_len != 3) {
        LOG_ERR("[virtio_blk_process_desc]: ERROR!!! Invalid desc_len(%d)\n", desc_len);
        return desc_len;
    }

    /* process blk req */
    virt_blk_req = (struct virtio_blk_req*)virtio_guest_to_host(desc[DESC_IDX_BLK_REQ].addr);
    blk_num = virt_blk_req->sector / (BLOCK_SIZE / 512);
    is_write = (virt_blk_req->type == VIRTIO_BLK_T_OUT) ? 1 : 0;

    buf_addr = virtio_guest_to_host(desc[DESC_IDX_BUFFER].addr);

    LOG_INFO("[virtio_blk_process_desc]: %s blockno(%d) %s ramdisk\n",
             is_write ? "write": "read", blk_num, is_write ? "to" : "from");
    ret = ramdisk_rw(blk_num, buf_addr, is_write);

    /* setup process result */
    u8 *status_pa = (u8*)virtio_guest_to_host(desc[DESC_IDX_REQ_STATUS].addr);
    if (ret == 0) {
        *status_pa = 0;
    } else {
        *status_pa = 0xee;  /* indicate process virtio req failed */
    }

    return desc_len;
}

static void virtio_blk_req_handler(void)
{
    u16 desc_idx = 0;
    u16 desc_len = 0;
    LOG_INFO("[virtio_blk_req_handler]: g_vq.avail_idx=%d, g_vq.avail->idx=%d\n",
           g_vq.avail_idx, g_vq.avail->idx);

    spin_lock(&g_vq.virtq_lock);
    while (virtq_available()) {
        /* fetch VM's virtio request */
        desc_idx = g_vq.avail->ring[g_vq.avail_idx % g_vq.vring_num];
        LOG_INFO("## [virtio_blk_req_handler]: ready to process desc[%d]\n", desc_idx);
        
        /* do real block request job */
        desc_len = virtio_blk_process_desc(desc_idx);

        /* update vring's used[], recording already processed desc elements */
        vqueue_set_used_elem(desc_idx, desc_len);

        ++g_vq.avail_idx;
    }
    spin_unlock(&g_vq.virtq_lock);

    /* all available desc elements are processed, now we inject irq to wakeup VM */
    virtio_signal_vq();
}

static int virtio_mmio_read(struct vcpu *vcpu, u64 offset,
                           u64 *val, struct mmio_access *mmio)
{
    LOG_INFO("[virtio_mmio_read]: offset=%p, ipa=%p, vm's pc=%p\n",
           offset, mmio->ipa, mmio->pc);

    switch (offset) {
        case VIRTIO_MMIO_MAGIC_VALUE:  // 0x74726976
            *val = 0x74726976;
            break;
        case VIRTIO_MMIO_VERSION:      // version; 1 is legacy
            *val = 1;
            break;
        case VIRTIO_MMIO_DEVICE_ID:  // device type; 1 is net, 2 is disk
            *val = 2;
            break;
        case VIRTIO_MMIO_VENDOR_ID: // 0x554d4551
            *val = 0x554d4551;
            break;
        case VIRTIO_MMIO_DEVICE_FEATURES:
            *val = 0;
            break;
        case VIRTIO_MMIO_QUEUE_NUM_MAX:  // max size of current queue, read-only
            *val = 64;
            break;
        case VIRTIO_MMIO_QUEUE_PFN: // physical page number for queue, read/write

            break;
        case VIRTIO_MMIO_QUEUE_READY: // ready bit

            break;
        case VIRTIO_MMIO_INTERRUPT_STATUS:  // read-only

            break;
        case VIRTIO_MMIO_STATUS:     // read/write
            
            break;
        default:
            panic("[virtio_mmio_read] Invalid/Unsupported offset(%p), ipa=%p, vm=%s, vcpuid=%d, vm's pc=%p\n",
                  offset, mmio->ipa, vcpu->vm->name, vcpu->cpuid, mmio->pc);
    }

    return 0;
}

static int virtio_mmio_write(struct vcpu *vcpu, u64 offset,
                            u64 val, struct mmio_access *mmio)
{
    LOG_INFO("[virtio_mmio_write]: offset=%p, ipa=%p, vm's pc=%p\n",
             offset, mmio->ipa, mmio->pc);

    switch (offset) {
        case VIRTIO_MMIO_DRIVER_FEATURES:
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_DRIVER_FEATURES feature=%p\n", val);
            break;
        case VIRTIO_MMIO_GUEST_PAGE_SIZE: // page size for PFN, write-only
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_GUEST_PAGE_SIZE guest_pagesz=%p\n", val);
            if (val != PAGE_SIZE) {
                panic("[virtio_mmio_write]: invalid page size(%p)\n", val);
            }
            guest_pagesz = val;
            break;
        case VIRTIO_MMIO_QUEUE_SEL: // select queue, write-only
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_QUEUE_SEL queue_sel=%p\n", val);
            g_queue_sel = val;
            break;
        case VIRTIO_MMIO_QUEUE_NUM:	 // size of current queue, write-only
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_QUEUE_NUM queue_num=%p\n", val);
            g_vq.vring_num = val;
            break;
        case VIRTIO_MMIO_QUEUE_ALIGN: // used ring alignment, write-only
        
            break;
        case VIRTIO_MMIO_QUEUE_PFN:	 // physical page number for queue, read/write
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_QUEUE_PFN queue_pfn's gpa=%p(pfn=%d)\n",
                    val << 12, val);
            vq_ring_init(val << 12);
            break;
        case VIRTIO_MMIO_QUEUE_READY: // ready bit

            break;
        case VIRTIO_MMIO_QUEUE_NOTIFY: // write-only
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_QUEUE_NOTIFY\n");
            virtio_blk_req_handler();
            break;
        case VIRTIO_MMIO_INTERRUPT_ACK: // write-only

            break;
        case VIRTIO_MMIO_STATUS:		 // read/write
            LOG_INFO("[virtio_mmio_write]: VIRTIO_MMIO_STATUS val=%p\n", val);
            break;
        default:
            panic("[virtio_mmio_write]: Invalid/Unsupported offset(%p), ipa=%p, vm=%s, vcpuid=%d, vm's pc=%p\n",
                  offset, mmio->ipa, vcpu->vm->name, vcpu->cpuid, mmio->pc);
    }
    return 0;
}

void virtio_mmio_init(struct vm *vm)
{
    s2_pt_trap(vm, VIRTIO0, VIRTIO0_SIZE, virtio_mmio_read, virtio_mmio_write);
}