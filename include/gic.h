#ifndef HYPER_POC_GIC_H
#define HYPER_POC_GIC_H

#include "types.h"
#include "memmap.h"

#define GIC_NSGI     16
#define GIC_SGI_MAX  15
#define GIC_NPPI     16
#define GIC_PPI_MAX  31

#define GIC_SPI0_INTNO 32

#define is_sgi(intid)     (0 <= (intid) && (intid) < 16)
#define is_ppi(intid)     (16 <= (intid) && (intid) < 32)
#define is_sgi_ppi(intid) (is_sgi(intid) || is_ppi(intid))
#define is_spi(intid)     (32 <= (intid))

#define ich_hcr_el2   arm_sysreg(4, c12, c11, 0)
#define ich_vtr_el2   arm_sysreg(4, c12, c11, 1)
#define ich_vmcr_el2  arm_sysreg(4, c12, c11, 7)
#define ich_lr0_el2   arm_sysreg(4, c12, c12, 0)
#define ich_lr1_el2   arm_sysreg(4, c12, c12, 1)
#define ich_lr2_el2   arm_sysreg(4, c12, c12, 2)
#define ich_lr3_el2   arm_sysreg(4, c12, c12, 3)
#define ich_lr4_el2   arm_sysreg(4, c12, c12, 4)
#define ich_lr5_el2   arm_sysreg(4, c12, c12, 5)
#define ich_lr6_el2   arm_sysreg(4, c12, c12, 6)
#define ich_lr7_el2   arm_sysreg(4, c12, c12, 7)
#define ich_lr8_el2   arm_sysreg(4, c12, c13, 0)
#define ich_lr9_el2   arm_sysreg(4, c12, c13, 1)
#define ich_lr10_el2  arm_sysreg(4, c12, c13, 2)
#define ich_lr11_el2  arm_sysreg(4, c12, c13, 3)
#define ich_lr12_el2  arm_sysreg(4, c12, c13, 4)
#define ich_lr13_el2  arm_sysreg(4, c12, c13, 5)
#define ich_lr14_el2  arm_sysreg(4, c12, c13, 6)
#define ich_lr15_el2  arm_sysreg(4, c12, c13, 7)

#define icc_pmr_el1       arm_sysreg(0, c4, c6, 0)
#define icc_eoir0_el1     arm_sysreg(0, c12, c8, 1)
#define icc_dir_el1       arm_sysreg(0, c12, c11, 1)
#define icc_sgi1r_el1     arm_sysreg(0, c12, c11, 5)
#define icc_iar1_el1      arm_sysreg(0, c12, c12, 0)
#define icc_eoir1_el1     arm_sysreg(0, c12, c12, 1)
#define icc_ctlr_el1      arm_sysreg(0, c12, c12, 4)
#define icc_sre_el1       arm_sysreg(0, c12, c12, 5)
#define icc_igrpen0_el1   arm_sysreg(0, c12, c12, 6)
#define icc_igrpen1_el1   arm_sysreg(0, c12, c12, 7)

#define icc_sre_el2       arm_sysreg(4, c12, c9, 5)

#define ICC_CTLR_EOImode(m) ((m) << 1)

#define ICC_SGI1R_TargetList(v)   ((v) & 0xffff)
#define ICC_SGI1R_INTID(v)        (((v)>>24) & 0xf)
#define ICC_SGI1R_IRM(v)          (((v)>>40) & 0x1)

#define ICH_HCR_EN  (1<<0)

#define ICH_VMCR_VENG0  (1<<0)
#define ICH_VMCR_VENG1  (1<<1)

#define ICH_LR_VINTID(n)   ((n) & 0xffffffffL)
#define ICH_LR_PINTID(n)   (((n) & 0x1fffL) << 32)
#define ICH_LR_GROUP(n)    (((n) & 0x1L) << 60)
#define ICH_LR_HW          (1L << 61)
#define ICH_LR_STATE(n)    (((n) & 0x3L) << 62)
#define LR_INACTIVE  0L
#define LR_PENDING   1L
#define LR_ACTIVE    2L
#define LR_PENDACT   3L
#define LR_MASK      3L

#define lr_is_inactive(lr)  (((lr >> 62) & 0x3) == LR_INACTIVE)
#define lr_is_pending(lr)   (((lr >> 62) & 0x3) == LR_PENDING)
#define lr_is_active(lr)    (((lr >> 62) & 0x3) == LR_ACTIVE)
#define lr_is_pendact(lr)   (((lr >> 62) & 0x3) == LR_PENDACT)

#define GICD_CTLR           (0x0)
#define GICD_CTLR_ENGRP(grp)    (1<<(grp))
#define GICD_TYPER          (0x4)
#define GICD_IIDR           (0x8)
#define GICD_TYPER2         (0xc)
#define GICD_IGROUPR(n)     (0x080 + (u64)(n) * 4)
#define GICD_ISENABLER(n)   (0x100 + (u64)(n) * 4)
#define GICD_ICENABLER(n)   (0x180 + (u64)(n) * 4)
#define GICD_ISPENDR(n)     (0x200 + (u64)(n) * 4)
#define GICD_ICPENDR(n)     (0x280 + (u64)(n) * 4)
#define GICD_ISACTIVER(n)   (0x300 + (u64)(n) * 4)
#define GICD_ICACTIVER(n)   (0x380 + (u64)(n) * 4)
#define GICD_IPRIORITYR(n)  (0x400 + (u64)(n) * 4)
#define GICD_ITARGETSR(n)   (0x800 + (u64)(n) * 4)
#define GICD_ICFGR(n)       (0xc00 + (u64)(n) * 4)
#define GICD_IROUTER(n)     (0x6000 + (u64)(n) * 8)
#define GICD_PIDR2          (0xffe8)

#define GICD_TYPER_CPUNum_SHIFT   5
#define GICD_TYPER_IDbits_SHIFT   19

#define GICD_IIDR_Revision_SHIFT    12
#define GICD_IIDR_ProductID_SHIFT   24

#define GICD_PIDR2_ArchRev(pidr2)   (((pidr2)>>4) & 0xf)
#define GICD_PIDR2_ArchRev_SHIFT    4

#define GICD_CTLR_G0_EN         (1 << 0)
#define GICD_CTLR_G1NS_EN       (1 << 1)
#define GICD_CTLR_G1S_EN        (1 << 2)
#define GICD_CTLR_ARE_S         (1 << 4)
#define GICD_CTLR_ARE_NS        (1 << 5)

/* Non-secure access in double security state */
#define GICD_CTLR_NS_ENGRP1     (1 << 0)
#define GICD_CTLR_NS_ENGRP1A    (1 << 1)
#define GICD_CTLR_NS_ARE_NS     (1 << 4)

/* only single security state */
#define GICD_CTLR_SS_ENGRP0     (1 << 0)
#define GICD_CTLR_SS_ENGRP1     (1 << 1)
#define GICD_CTLR_SS_ARE        (1 << 4)
#define GICD_CTLR_DS            (1 << 6)

#define GIC_GICD_INT_PER_REG                (32)     /* 32 interrupts per reg */
#define GIC_GICD_IPRIORITY_PER_REG          (4)      /* 4 priority per reg */
#define GIC_GICD_IPRIORITY_SIZE_PER_REG     (8)      /* priority element size */
#define GIC_GICD_ITARGETSR_PER_REG          (4)
#define GIC_GICD_ITARGETSR_SIZE_PER_REG     (8)
#define GIC_GICD_ICFGR_PER_REG              (16)
#define GIC_GICD_ICFGR_SIZE_PER_REG         (2)
#define GIC_GICD_ICENABLER_PER_REG          (32)
#define GIC_GICD_ISENABLER_PER_REG          (32)
#define GIC_GICD_ICPENDR_PER_REG            (32)
#define GIC_GICD_ISPENDR_PER_REG            (32)
#define GIC_GICD_ICACTIVER_PER_REG          (32)
#define GIC_GICD_IGROUPR_PER_REG            (32)

#define GIC_PRIO_SHIFT                      (4)
#define GIC_PRIO_MASK                       (0x0f)

#define GIC_GICD_ICFGR_LEVEL                (0x0)     /* level-sensitive */
#define GIC_GICD_ICFGR_EDGE                 (0x2)     /* edge-triggered */

#define GIC_GICD_IGROUPR_DEF                (0xFFFFFFFF)
#define GIC_INT_DEF_PRIO                    0xA0A0A0A0

#define GICRBASEn(n)                        (GICRBASE + (n) * GICRSTRIDE)

#define GICR_CTLR           (0x0)
#define GICR_IIDR           (0x4)
#define GICR_TYPER          (0x8)
#define GICR_WAKER          (0x14)
#define GICR_PIDR2          (0xffe8)

/* SGI_base is at 64K offset from Redistributor */
#define SGI_BASE            (0x10000)
#define GICR_IGROUPR0       (SGI_BASE+0x80)
#define GICR_ISENABLER0     (SGI_BASE+0x100)
#define GICR_ICENABLER0     (SGI_BASE+0x180)
#define GICR_ISPENDR0       (SGI_BASE+0x200)
#define GICR_ICPENDR0       (SGI_BASE+0x280)
#define GICR_ISACTIVER0     (SGI_BASE+0x300)
#define GICR_ICACTIVER0     (SGI_BASE+0x380)
#define GICR_IPRIORITYR(n)  (SGI_BASE+0x400+(n)*4)
#define GICR_ICFGR0         (SGI_BASE+0xc00)
#define GICR_ICFGR1         (SGI_BASE+0xc04)
#define GICR_IGRPMODR0      (SGI_BASE+0xd00)

#define GIC_GICR_WAKER_PS   (1u << 1)
#define GIC_GICR_WAKER_CA   (1u << 2)

#define ICC_SRE_EL1_SRE     (1U << 0)
#define ICC_SRE_EL2_SRE     (1U << 0)
#define ICC_SRE_EL2_Enable  (1U << 3)
#define GIC_IDLE_PRIO       (0xff)    /* allow all interrupts */

static inline u32 gicd_r(u32 offset) {
  return *(volatile u32 *)(u64)(GICDBASE + offset);
}

static inline void gicd_w(u32 offset, u32 val) {
  *(volatile u32 *)(u64)(GICDBASE + offset) = val;
}

static inline u32 gicr_r32(int cpuid, u32 offset) {
  return *(volatile u32 *)(u64)(GICRBASEn(cpuid) + offset);
}

static inline void gicr_w32(int cpuid, u32 offset, u32 val) {
  *(volatile u32 *)(u64)(GICRBASEn(cpuid) + offset) = val;
}

static inline u64 gicr_r64(int cpuid, u32 offset) {
  return *(volatile u64 *)(u64)(GICRBASEn(cpuid) + offset);
}

static inline void gicr_w64(int cpuid, u32 offset, u32 val) {
  *(volatile u64 *)(u64)(GICRBASEn(cpuid) + offset) = val;
}

/* get bit or get bits from register */
#define REG_GET_BIT(_r, _b)  ({                    \
            (*(volatile u32*)(_r) & (_b));    \
        })

/* set bit or set bits to register */
#define REG_SET_BIT(_r, _b)  ({                    \
            (*(volatile u32*)(_r) |= (_b));   \
        })

/* clear bit or clear bits of register */
#define REG_CLR_BIT(_r, _b)  ({                    \
            (*(volatile u32*)(_r) &= ~(_b));  \
        })

struct gic_state {
    u64 lr[16];
    u64 vmcr_el2;   /* Interrupt Controller Virtual Machine Control Register */
    u32 sre_el1;    /* enable access to GICC system register, which is configured by VM */
};

void gicv3_init();
void init_gicv3_percpu();
int gic_is_irq_enable(u32 irq);

void vm_gic_state_init(struct gic_state *gic_state);

u32 gic_read_iar(void);
int gic_max_spi(void);

void gic_deactive_irq(u32 irq);

void gic_host_eoi(u32 irq, int group);
void gic_guest_eoi(u32 irq, int group);

void gic_set_target_by_affinity(u32 irq, u8 aff);
void gic_set_target_by_pe_field(u32 irq, u8 target);

bool gic_has_pending_lr(void);
u64 gic_read_lr(int n);
void gic_write_lr(int n, u64 val);
u64 gic_make_lr(u32 pirq, u32 virq, int group);

void gic_irq_enable(u32 irq);
void gic_irq_disable(u32 irq);
void gic_irq_enable_redist(u32 cpuid, u32 irq);

void gic_restore_state(struct gic_state *gic);

void gic_set_pending_irq(u16 irq_id);

#endif
