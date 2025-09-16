#include "vgic.h"
#include "vm.h"
#include "vcpu.h"
#include "page_alloc.h"
#include "gic.h"
#include "types.h"
#include "debug.h"

extern u32 g_gic_lr_max;

static struct vgic g_vgic[VM_MAX];
static struct vgic_cpu g_vgic_cpu[VCPU_MAX];
static spinlock_t g_vgic_lock;
static spinlock_t g_vgic_cpu_lock;

static struct vgic *vgic_alloc()
{
    spin_lock(&g_vgic_cpu_lock);
    for (int i = 0; i < VM_MAX; ++i) {
        if (g_vgic[i].used == 0) {
            g_vgic[i].used = 1;
            spin_unlock(&g_vgic_cpu_lock);
            return &g_vgic[i];
        }
    }
    spin_unlock(&g_vgic_cpu_lock);
    return NULL;
};


static struct vgic_cpu *vgic_cpu_alloc()
{
    spin_lock(&g_vgic_cpu_lock);
    for (int i = 0; i < VCPU_MAX; ++i) {
        if (g_vgic_cpu[i].used == 0) {
            g_vgic_cpu[i].used = 1;
            spin_unlock(&g_vgic_cpu_lock);
            return &g_vgic_cpu[i];
        }
    }
    spin_unlock(&g_vgic_cpu_lock);
    return NULL;
}

static int vgic_lr_alloc(struct vgic_cpu *vgic_cpu)
{
    for (int i = 0; i < g_gic_lr_max; ++i) {
        if (!(vgic_cpu->used_lr & (1 << i))) {
            vgic_cpu->used_lr |= (1 << i);
            return i;
        }
    }
    LOG_WARN("[vgic_lr_alloc]: WARNING!! No available LR\n");
    return -1;
}

void vgic_used_lr_update(struct vcpu *vcpu)
{
    struct vgic_cpu *vgic_cpu = vcpu->vgic;
    for (int i = 0; i < g_gic_lr_max; ++i) {
        if (vgic_cpu->used_lr & (1 << i)) {
            u64 lr = gic_read_lr(i);
            if (lr_is_inactive(lr)) {
                vgic_cpu->used_lr &= ~(1 << i);
            }
        }
    }
}

static struct vgic_irq *vgic_irq_get(struct vcpu *vcpu, int intid)
{
    if (is_sgi(intid)) {
        return &vcpu->vgic->sgis[intid];
    } else if (is_ppi(intid)) {
        return &vcpu->vgic->ppis[intid - 16];
    } else if (is_spi(intid)) {
        return &vcpu->vm->vgic->spis[intid - 32];
    } else {
        LOG_ERR("[vgic_irq_get]: invalid intid=%d, vm=%s\n", intid, vcpu->vm->name);
    }

    return NULL;
}

static void vgic_irq_enable(int intid)
{
    if (is_sgi_ppi(intid) || is_spi(intid)) {
        gic_irq_enable(intid);
    } else {
        panic("[vgic_irq_enable] invalid intid=%d\n", intid);
    }
}

static void vgic_irq_disable(int intid)
{
    if (is_sgi_ppi(intid) || is_spi(intid)) {
        gic_irq_disable(intid);
    } else {
        panic("[vgic_irq_disable] invalid intid=%d\n", intid);
    }
}

static int vgicd_mmio_read(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio)
{
    int ret = 0;
    int intid;
    u64 val64 = 0;
    struct vgic_irq *vgic_irq;
    struct vgic *vgic = vcpu->vm->vgic;

    LOG_INFO("[vgicd_mmio_read]: offset=0x%x\n", offset);
    switch (offset) {
        case GICD_CTLR:
        {
            spin_lock(&g_vgic_lock);
            u64 ctlr = 0;
            ctlr = (vgic->enable_grp1ns) ? GICD_CTLR_G1NS_EN : 0;
            ctlr |= GICD_CTLR_ARE_NS | GICD_CTLR_G1NS_EN;
            spin_unlock(&g_vgic_lock);
            *val = ctlr;
            LOG_INFO("[vgicd_mmio_read] read GICD_CTLR, val=0x%x\n", *val);
            break;
        }
        case GICD_TYPER:
            *val = (u64)gicd_r(GICD_TYPER);
            LOG_INFO("[vgicd_mmio_read] read GICD_TYPER, val=0x%x\n", *val);
            break;
        case GICD_IIDR:
            *val = (u64)gicd_r(GICD_IIDR);
            LOG_INFO("[vgicd_mmio_read] read GICD_IIDR, val=0x%x\n", *val);
            break;
        case GICD_TYPER2:
            *val = (u64)gicd_r(GICD_TYPER2);
            LOG_INFO("[vgicd_mmio_read] read GICD_TYPER2, val=0x%x\n", *val);
            break;
        case GICD_IGROUPR(0) ... GICD_IGROUPR(31):
            //【TODO】
            // 目前hyper针对VM读取GICD_IGROUPR<n>的操作不做处理, 默认所有VM的irq都是group1的
            // 后续要改造成支持VM对GICD_IGROUPR<n>的读写操作, 不过要考虑虚拟化场景下, GIC硬件中的配置
            // 到底是完全由VM的配置来决定, 还是说VM只能配置部分寄存器, 其他寄存器由hypervisor来配置 ？？？
            *val = 0xffffffff;
            LOG_INFO("[vgicd_mmio_read] read GICD_IGROUPR<%d>, val=0x%x\n",
                     (offset - GICD_IGROUPR(0)) / sizeof(u32), *val);
            break;
        case GICD_ISENABLER(0) ... GICD_ISENABLER(31):
        {
            /* intid: 是GICD_ISENABLER<n>所代表中断号范围的起始中断号 */
            intid = (offset - GICD_ISENABLER(0)) / sizeof(u32) * 32;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 32; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                if (vgic_irq->enabled == 1) {
                    val64 |= (1 << i);
                }
            }
            spin_unlock(&g_vgic_lock);
            *val = val64;
            LOG_INFO("[vgicd_mmio_read] read GICD_ISENABLER<%d>, val=0x%x\n", 
                     (offset - GICD_ISENABLER(0)) / sizeof(u32), *val);
            break;
        }
        case GICD_ICENABLER(0) ... GICD_ICENABLER(31):
            /* intid: GICD_ICENABLER<n>所代表中断号范围的起始中断号
             *
             * [Clear_enable_bit]
             * - 0b0: If read, indicates that forwarding of the corresponding interrupt is disabled.
             * - 0b1: If read, indicates that forwarding of the corresponding interrupt is enabled.
             */
            intid = (offset - GICD_ICENABLER(0)) / sizeof(u32) * 32;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 32; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                if (vgic_irq->enabled == 0) {
                    val64 |= (0 << i);
                } else {
                    val64 |= (1 << i);
                }
            }
            spin_unlock(&g_vgic_lock);
            *val = val64;
            LOG_INFO("[vgicd_mmio_read] read GICD_ICENABLER<%d>, val=0x%x\n",
                     (offset - GICD_ICENABLER(0)) / sizeof(u32), *val);
            break;
        // TODO: 这里VM对GICD_ISPENDR/ICPENDR/ISACTIVER/ICACTIVER的读操作, 这些寄存器的信息
        //       应该记录在vgic_irq中, 但是目前还没有实现。我理解这些寄存器的值, 不是直接从真实
        //       的GIC硬件中读取的, 而是从hyper维护的vgic_irq中获取
        case GICD_ISPENDR(0) ... GICD_ISPENDR(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_ISPENDR<%d> unsupported yet\n",
                     (offset - GICD_ISPENDR(0)) / sizeof(u32));
            break;
        case GICD_ICPENDR(0) ... GICD_ICPENDR(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_ICPENDR<%d> unsupported yet\n",
                     (offset - GICD_ICPENDR(0)) / sizeof(u32));
            break;
        case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_ISACTIVER<%d> unsupported yet\n",
                     (offset - GICD_ISACTIVER(0)) / sizeof(u32));
            break;
        case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_ICACTIVER<%d> unsupported yet\n",
                     (offset - GICD_ICACTIVER(0)) / sizeof(u32));
            break;
        case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254):
            intid = (offset - GICD_IPRIORITYR(0)) / sizeof(u32) * 4;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 4; ++i) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                val64 |= ((u32)vgic_irq->priority) << (i * 8);
            }
            spin_unlock(&g_vgic_lock);
            *val = val64;
            LOG_INFO("[vgicd_mmio_read] read GICD_IPRIORITYR<%d>, val=0x%x\n",
                     (offset - GICD_IPRIORITYR(0)) / sizeof(u32), *val);
            break;
        case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254):
            intid = (offset - GICD_ITARGETSR(0)) / sizeof(u32) * 4;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 4; ++i) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                val64 |= ((u32)vgic_irq->target) << (i * 8);
            }
            spin_unlock(&g_vgic_lock);
            *val = val64;
            LOG_INFO("[vgicd_mmio_read] read GICD_ITARGETSR<%d>, val=0x%x\n",
                     (offset - GICD_ITARGETSR(0)) / sizeof(u32), *val);
            break;
        case GICD_ICFGR(0) ... GICD_ICFGR(63):
            /* Control whether interrupt is edge-triggered or level-sensitive */
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_ICFGR<%d> not supported yet\n",
                     (offset - GICD_ICFGR(0)) / sizeof(u32));
            break;
        case GICD_IROUTER(32) ... GICD_IROUTER(1019):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! read GICD_IROUTER<%d> not supported yet\n",
                     (offset - GICD_IROUTER(32)) / sizeof(u32));
            // TODO: Check whether use GICD_ITARGETSR or GICD_IROUTER to set irq target in gicv3
            //       GICD_ITARGETSR is only used in gicv2 ?
            break;
        default:
            LOG_WARN("[vgicd_mmio_read]: unknown/unsupported offset 0x%x\n", offset);
            break;
    }
    return ret;
}


static int vgicd_mmio_write(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio)
{
    int ret = 0;
    int intid;
    struct vgic_irq *vgic_irq;
    struct vgic *vgic = vcpu->vm->vgic;

    LOG_INFO("[vgicd_mmio_write]: offset=0x%x, val=0x%x\n", offset, val);
    switch (offset) {
        case GICD_CTLR:
            vgic->enable_grp1ns = (val & GICD_CTLR_G1NS_EN) ? true : false;
            break;
        case GICD_TYPER:
        case GICD_IIDR:
        case GICD_TYPER2:
            LOG_WARN("[vgicd_mmio_write] WARNING!!! GICD offset(%d) is RO\n", offset);
            break;
        case GICD_IGROUPR(0) ... GICD_IGROUPR(31):
            LOG_INFO("[vgicd_mmio_write] write GICD_IGROUPR<%d>, val=0x%x\n",
                     (offset - GICD_IGROUPR(0)) / sizeof(u32), val);
            /* TODO: 这里hyper的处理默认所有VM中断都是group1的, 这里VM的写操作不作处理, 直接返回 */
            break;
        case GICD_ISENABLER(0) ... GICD_ISENABLER(31):
            LOG_INFO("[vgicd_mmio_write] write GICD_ISENABLER<%d>, val=0x%x\n",
                     (offset - GICD_ISENABLER(0)) / sizeof(u32), val);
            /*
             * intid: GICD_ISENABLER<n>代表中断号范围的起始中断号, 该变量的值由offset计算得到的n决定
             */
            intid = (offset - GICD_ISENABLER(0)) / sizeof(u32) * 32;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 32; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                if ((val >> i) & 0x1) {
                    vgic_irq->enabled = 1;
                    LOG_INFO("[vgicd_mmio_write] write GICD_ISENABLER to enable irq %d\n", intid + i);
                    vgic_irq_enable(intid + i);
                }
            }
            spin_unlock(&g_vgic_lock);
            break;
        case GICD_ICENABLER(0) ... GICD_ICENABLER(31):
            LOG_INFO("[vgicd_mmio_write] write GICD_ICENABLER<%d>, val=0x%x\n",
                     (offset - GICD_ICENABLER(0)) / sizeof(u32), val);
            intid = (offset - GICD_ICENABLER(0)) / sizeof(u32) * 32;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 32; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                if ((val >> i) & 0x1) {
                    vgic_irq->enabled = 0;
                    vgic_irq_disable(intid + i);
                }
            }
            spin_unlock(&g_vgic_lock);
            break;
        case GICD_ISPENDR(0) ... GICD_ISPENDR(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! write GICD_ISPENDR<%d> unsupported yet\n",
                     (offset - GICD_ISPENDR(0)) / sizeof(u32));
            break;
        case GICD_ICPENDR(0) ... GICD_ICPENDR(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! write GICD_ICPENDR<%d> unsupported yet\n",
                     (offset - GICD_ICPENDR(0)) / sizeof(u32));
            break;
        case GICD_ISACTIVER(0) ... GICD_ISACTIVER(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! write GICD_ISACTIVER<%d> unsupported yet\n",
                     (offset - GICD_ISACTIVER(0)) / sizeof(u32));
            break;
        case GICD_ICACTIVER(0) ... GICD_ICACTIVER(31):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! write GICD_ICACTIVER<%d> unsupported yet\n",
                     (offset - GICD_ICACTIVER(0)) / sizeof(u32));
            break;
        case GICD_IPRIORITYR(0) ... GICD_IPRIORITYR(254):
            LOG_INFO("[vgicd_mmio_write] write GICD_IPRIORITYR<%d>, val=0x%x\n",
                     (offset - GICD_IPRIORITYR(0)) / sizeof(u32), val);
            intid = (offset - GICD_IPRIORITYR(0)) / sizeof(u32) * 4;
            for (int i = 0; i < 4; ++i) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                vgic_irq->priority = (val >> (i * 8)) & 0xff;
            }
            break;
        case GICD_ITARGETSR(0) ... GICD_ITARGETSR(254):
            LOG_INFO("[vgicd_mmio_write] write GICD_ITARGETSR<%d>, val=0x%x\n",
                     (offset - GICD_ITARGETSR(0)) / sizeof(u32), val);
            intid = (offset - GICD_ITARGETSR(0)) / sizeof(u32) * 4;
            spin_lock(&g_vgic_lock);
            for (int i = 0; i < 4; ++i) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                vgic_irq->target = (u8)((val >> (i * 8)) & 0xff);
                if (is_spi(intid + i)) {
                    gic_set_target_by_pe_field(intid + i, vgic_irq->target);
                } else {
                    spin_unlock(&g_vgic_lock);
                    panic("[vgicd_mmio_write] invalid intid=%d\n", intid + i);
                }
            }
            spin_unlock(&g_vgic_lock);
            break;
        case GICD_ICFGR(0) ... GICD_ICFGR(63):
            LOG_INFO("[vgicd_mmio_read] WARNING!!! write GICD_ICFGR<%d> not supported yet\n",
                     (offset - GICD_ICFGR(0)) / sizeof(u32));
            break;
        case GICD_IROUTER(32) ... GICD_IROUTER(1019):
            LOG_INFO("[vgicd_mmio_write] write GICD_IROUTER<%d> val = 0x%x\n",
                     (offset - 0x6000) / 8, val);
            gic_set_target_by_affinity((offset - 0x6000) / 8, val);
            break;
        default:
            LOG_WARN("[vgicd_mmio_write]: unknown offset 0x%x\n", offset);
            break;
    }
    return ret;
}


static int vgicr_mmio_read(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio)
{
    int ret = 0;
    int intid = 0;
    u64 val64 = 0;
    struct vgic_irq *vgic_irq = NULL;
    u32 gicr_idx = offset / GICRSTRIDE;
    u32 gicr_off = offset % GICRSTRIDE;

    if (gicr_idx > vcpu->vm->nvcpu - 1) {
        LOG_ERR("[vgicr_mmio_read] ERROR: invalid gicr_idx=%d, nvcpu=%d, offset=0x%x\n",
               gicr_idx, vcpu->vm->nvcpu, offset);
        return -1;
    }
    LOG_INFO("[vgicr_mmio_read]: offset=0x%x, gicr_idx=%d, gicr_off=0x%x\n",
             offset, gicr_idx, gicr_off);

    // TODO: 
    // 当hyper支持多个VM时, 需要实现根据vcpu_id来获取vcpu对象的方式！
    vcpu = vcpu->vm->vcpus[gicr_idx];

    switch (gicr_off) {
        case GICR_CTLR:
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] read GICR_CTLR, return 0\n");
            break;
        case GICR_IIDR:
            *val = gicr_r64(vcpu->cpuid, GICR_IIDR);
            LOG_INFO("[vgicr_mmio_read] read GICR_IIDR, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_TYPER:
            *val = gicr_r64(vcpu->cpuid, GICR_TYPER);
            LOG_INFO("[vgicr_mmio_read] read GICR_TYPER, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_WAKER:
            /* used during enable gicr, gicr is already waked up by hypervisor */
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] read GICR_WAKER, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_IGROUPR0:
            *val = (u64)0xffffffff;
            LOG_INFO("[vgicr_mmio_read] read GICR_IGROUPR0, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ISENABLER0:
        case GICR_ICENABLER0:
            for (int i = 0; i < 32; ++i) {
                vgic_irq = vgic_irq_get(vcpu, i);
                if (vgic_irq->enabled == 1) {
                    val64 |= 1 << i;
                }
            }
            *val = val64;
            LOG_INFO("[vgicr_mmio_read] read %s, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     (offset == GICR_ISENABLER0 ? "GICR_ISENABLER0" : "GICR_ICENABLER0"),
                     *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ISPENDR0:
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] WARNING!!! read GICR_ISPENDR0 unsupported yet"
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     offset, gicr_idx, gicr_off);
            break;
        case GICR_ICPENDR0:
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] WARNING!!! read GICR_ICPENDR0 unsupported yet"
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     offset, gicr_idx, gicr_off);
            break;
        case GICR_ISACTIVER0:
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] WARNING!!! read GICR_ISACTIVER0 unsupported yet"
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     offset, gicr_idx, gicr_off);
            break;
        case GICR_ICACTIVER0:
            *val = 0;
            LOG_INFO("[vgicr_mmio_read] WARNING!!! read GICR_ICACTIVER0 unsupported yet"
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     offset, gicr_idx, gicr_off);
            break;
        case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):
            /* GICR_IPRIORITYR0-GICR_IPRIORITYR3 store the priority of SGIs.
             * GICR_IPRIORITYR4-GICR_IPRIORITYR7 store the priority of PPIs. */
            intid = (offset - GICR_IPRIORITYR(0)) / sizeof(u32) * 4;
            for(int i = 0; i < 4; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                val64 |= vgic_irq->priority << (i * 8);
            }
            *val = val64;
            LOG_INFO("[vgicr_mmio_read] read GICR_IPRIORITYR<%d>, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     (offset - GICR_IPRIORITYR(0)) / sizeof(u32), *val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ICFGR0:
            /* SGIs are always edge-triggered */
            for (int i = 0; i < 16; ++i) {
                val64 |= (0b10 << i);
            }
            *val = val64;
            LOG_INFO("[vgicr_mmio_read] read GICR_ICFGR0, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     *val, offset, gicr_idx, gicr_off);
            break;
        default:
            LOG_INFO("[vgicr_mmio_read]: unknown/unsupported offset 0x%x, gicr_idx=%d, gicr_off=0x%x\n",
                     offset, gicr_idx, gicr_off);
            ret = -1;
            break;
    }

    return ret;
}


static int vgicr_mmio_write(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio)
{
    int ret = 0;
    int intid = 0;
    struct vgic_irq *vgic_irq = NULL;
    u32 gicr_idx = offset / GICRSTRIDE;
    u32 gicr_off = offset % GICRSTRIDE;
    if (gicr_idx > vcpu->vm->nvcpu - 1) {
        LOG_ERR("[vgicr_mmio_write] ERROR: invalid gicr_idx=%d, nvcpu=%d, offset=0x%x, pcpu=%d\n",
                gicr_idx, vcpu->vm->nvcpu, offset, cpuid());
        return -1;
    }
    LOG_INFO("[vgicr_mmio_write]: offset=0x%x, gicr_idx=%d, gicr_off=0x%x\n",
             offset, gicr_idx, gicr_off);

    // TODO: 同vgicr_mmio_read
    vcpu = vcpu->vm->vcpus[gicr_idx];

    switch (gicr_off) {
        case GICR_CTLR:
            LOG_INFO("[vgicr_mmio_write] write GICR_CTLR, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_IIDR:
            LOG_WARN("[vgicr_mmio_write] WARNING!!! GICR_IIDR is RO, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_TYPER:
            LOG_WARN("[vgicr_mmio_write] WARNING!!! GICR_TYPER is RO, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_WAKER:
            /* used during enable gicr, gicr is already waked up by hypervisor */
            LOG_INFO("[vgicr_mmio_write] write GICR_WAKER, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_IGROUPR0:
            LOG_INFO("[vgicr_mmio_write] write GICR_IGROUPR0, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ISENABLER0:
            for (int i = 0; i < 32; ++i) {
                vgic_irq = vgic_irq_get(vcpu, i);
                if ((val >> i) & 0x1) {
                    vgic_irq->enabled = 1;
                    vgic_irq_enable(i);
                }
            }
            LOG_INFO("[vgicr_mmio_write] write GICR_ISENABLER0, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ICENABLER0:
            for (int i = 0; i < 32; ++i) {
                vgic_irq = vgic_irq_get(vcpu, i);
                if ((val >> i) & 0x1) {
                    vgic_irq->enabled = 0;
                    vgic_irq_disable(i);
                }
            }
            LOG_INFO("[vgicr_mmio_write] write GICR_ICENABLER0, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ISPENDR0:
            LOG_INFO("[vgicr_mmio_write] WARNING!!! write GICR_ISPENDR0 unsupported yet, val=0x%x "
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ICPENDR0:
            LOG_INFO("[vgicr_mmio_write] WARNING!!! write GICR_ICPENDR0 unsupported yet, val=0x%x "
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ISACTIVER0:
            LOG_INFO("[vgicr_mmio_write] WARNING!!! write GICR_ISACTIVER0 unsupported yet, val=0x%x "
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ICACTIVER0:
            LOG_INFO("[vgicr_mmio_write] WARNING!!! write GICR_ICACTIVER0 unsupported yet, val=0x%x "
                     "(offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_IPRIORITYR(0) ... GICR_IPRIORITYR(7):
            /* GICR_IPRIORITYR0-GICR_IPRIORITYR3 store the priority of SGIs.
             * GICR_IPRIORITYR4-GICR_IPRIORITYR7 store the priority of PPIs. */
            intid = (offset - GICR_IPRIORITYR(0)) / sizeof(u32) * 4;
            for(int i = 0; i < 4; i++) {
                vgic_irq = vgic_irq_get(vcpu, intid + i);
                vgic_irq->priority = (val >> (i * 8)) & 0xff;
            }
            LOG_INFO("[vgicr_mmio_write] write GICR_IPRIORITYR<%d>, val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     (offset - GICR_IPRIORITYR(0)) / sizeof(u32), val, offset, gicr_idx, gicr_off);
            break;
        case GICR_ICFGR0:
            /* SGIs are always edge-triggered */
            LOG_INFO("[vgicr_mmio_write] write GICR_ICFGR0 val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        case GICR_IGRPMODR0:
            LOG_INFO("[vgicr_mmio_write] write GICR_ICFGR0 val=0x%x (offset=0x%x, gicr_idx=%d, gicr_off=0x%x)\n",
                     val, offset, gicr_idx, gicr_off);
            break;
        default:
            LOG_INFO("[vgicr_mmio_write]: unknown/unsupported offset 0x%x, gicr_idx=%d, gicr_off=0x%x, pcpu=%d\n",
                     offset, gicr_idx, gicr_off, cpuid());
            ret = -1;
            break;
    }

    return ret;
}

struct vgic *new_vgic(struct vm *vm)
{
    struct vgic *vgic = vgic_alloc();
    if (NULL == vgic) {
        panic("[new_vgic] no mem");
        return NULL;
    }

    vgic->max_spi_intid = gic_max_spi();
    vgic->spi_nums = vgic->max_spi_intid - 31;
    vgic->enable_grp1ns = 0;
    vgic->spis = (struct vgic_irq *)alloc_page();

    s2_pt_trap(vm, GICDBASE, GICDSIZE, vgicd_mmio_read, vgicd_mmio_write);
    s2_pt_trap(vm, GICRBASE, GICRSIZE, vgicr_mmio_read, vgicr_mmio_write);

    return vgic;
}

struct vgic_cpu *new_vgic_cpu(int vcpuid)
{
    struct vgic_cpu *vgic_cpu = vgic_cpu_alloc();
    if (NULL == vgic_cpu) {
        return NULL;
    }

    vgic_cpu->used_lr = 0;
    for (int i = 0; i < GIC_NSGI; ++i) {
        vgic_cpu->sgis[i].enabled = 1;
        vgic_cpu->sgis[i].target = vcpuid;
    }
    
    for (int i = 0; i < GIC_NPPI; ++i) {
        vgic_cpu->ppis[i].enabled = 0;
        vgic_cpu->ppis[i].target = vcpuid;
    }

    return vgic_cpu;
}

int vgic_inject_virq(struct vcpu *vcpu, u32 pirq, u32 virq, int group)
{
    struct vgic_cpu *vgic = vcpu->vgic;

    u64 lr_val = gic_make_lr(pirq, virq, group);

    int n = vgic_lr_alloc(vgic);
    if (n < 0) {
        /* TODO: 缓存该物理中断 */
        LOG_ERR("[vgic_inject_virq]: WARNING!!! failed to inject virq\n");
        return -1;
    }

    gic_write_lr(n, lr_val);

    // LOG_INFO("[vgic_inject_virq]: pirq=%d, virq=%d, group=%d, lr<%d>=0x%x, pcpu=%d\n",
    //        pirq, virq, group, n, gic_read_lr(n), cpuid());

    return 0;
}

void vgic_restore_state(struct vgic_cpu *vgic)
{
    return;
}

void vgic_init(void)
{
    spinlock_init(&g_vgic_lock);
    spinlock_init(&g_vgic_cpu_lock);
    for (int i = 0; i < VM_MAX; ++i) {
        g_vgic[i].used = 0;
        spinlock_init(&g_vgic[i].lock);
    }
    for (int i = 0; i < VCPU_MAX; ++i) {
        g_vgic_cpu[i].used = 0;
    }
}