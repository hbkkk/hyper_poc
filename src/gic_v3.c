#include "gic.h"
#include "aarch64.h"
#include "processor.h"
#include "debug.h"

#define GICD_NEED_INITIATE   1
#define GICR_NEED_INITIATE   1

u32 g_spi_num = 0;
u32 g_gic_lr_max = 0;

static void gic_redistributor_rwp()
{
    unsigned long rbase = GICRBASEn(cpuid());
    unsigned long gicr_ctlr = rbase + GICR_CTLR;
    u32 rwp_bit = 1u << 3;

    while(REG_GET_BIT(gicr_ctlr, rwp_bit))
        ;
}

static void gic_distributor_rwp()
{
    unsigned long base = GICDBASE;
    unsigned long gicd_ctlr = base + GICD_CTLR;
    u32 rwp_bit = 1u << 31;

    while(REG_GET_BIT(gicd_ctlr, rwp_bit))
        ;
}

static void gicv3_wait_for_rwp(int irq_no)
{
    if (irq_no < GIC_SPI0_INTNO) {
        gic_redistributor_rwp();
    } else {
        gic_distributor_rwp();
    }
}

static void init_gicd_v3()
{
    int regs_nr = 0;
    u64 affinity = 0;
    u32 gicd_ctlr_val = 0;
    u32 gicd_typer_val = 0;
    // LOG_INFO("orign gicd_ctlr=0x%x\n", gicd_r(GICD_CTLR));

    /* Disable non-secure Group 1 and Group 0 interrupts */
    gicd_ctlr_val = gicd_r(GICD_CTLR);
    gicd_ctlr_val &= ~(GICD_CTLR_SS_ENGRP0 | GICD_CTLR_SS_ENGRP1);
    gicd_w(GICD_CTLR, gicd_ctlr_val);
    gicv3_wait_for_rwp(GIC_SPI0_INTNO);

    /* fetch SPI max number */
    gicd_typer_val = gicd_r(GICD_TYPER);
    g_spi_num = ((gicd_typer_val & 0x1f) + 1) * 32;

/* 可能#if宏圈起来的这些针对Distributor的配置不需要由hypervisor来设置, 待确认... */
#if GICD_NEED_INITIATE
    /* disable SPI interrupts, clear SPI interrupts' pending state */
    regs_nr = (g_spi_num + GIC_GICD_INT_PER_REG -1) / GIC_GICD_INT_PER_REG;
    for (int i = GIC_SPI0_INTNO / GIC_GICD_ICENABLER_PER_REG; regs_nr > i; ++i) {
        gicd_w(GICD_ICENABLER(i), ~((u32)(0)));
        gicd_w(GICD_ICPENDR(i), ~((u32)(0)));
    }

    /* setup all SPIs' priority to 0xA0 */
    regs_nr = (g_spi_num + GIC_GICD_IPRIORITY_PER_REG -1) / GIC_GICD_IPRIORITY_PER_REG;
    for (int i = GIC_SPI0_INTNO / GIC_GICD_IPRIORITY_PER_REG; regs_nr > i; ++i) {
        gicd_w(GICD_IPRIORITYR(i), (0xA0u | (0xA0u << 8) | (0xA0u << 16) | (0xA0u << 24)));
    }

    /* setup all SPIs' trigger type to level-trigger */
    regs_nr = (g_spi_num + GIC_GICD_ICFGR_PER_REG -1) / GIC_GICD_ICFGR_PER_REG;
    for (int i = GIC_SPI0_INTNO / GIC_GICD_ICFGR_PER_REG; regs_nr > i; ++i) {
        gicd_w(GICD_ICFGR(i), GIC_GICD_ICFGR_LEVEL);
    }

    /* Set target of all of shared peripherals to processor 0 */
    affinity = 0;
    for (int i = GIC_SPI0_INTNO; i < g_spi_num; ++i) {
        gicd_w(GICD_IROUTER(i), affinity);
    }

    /* Set spi group 1 */
    regs_nr = (g_spi_num + GIC_GICD_IGROUPR_PER_REG -1) / GIC_GICD_IGROUPR_PER_REG;
#if 0
    for (int i = GIC_SPI0_INTNO / GIC_GICD_IGROUPR_PER_REG; regs_nr > i; ++i) {
        gicd_w(GICD_IGROUPR(i), GIC_GICD_IGROUPR_DEF);
    }
#else
    for(int i = 0; i < (gicd_typer_val & 0x1f); i++) {
        gicd_w(GICD_IGROUPR(i), ~0);
    }
#endif
#endif
    /* Enable non-secure Group 1 and Group 0 interrupts */
    gicd_ctlr_val = gicd_r(GICD_CTLR);
    gicd_ctlr_val |= (GICD_CTLR_SS_ENGRP0 | GICD_CTLR_SS_ENGRP1);
    gicd_w(GICD_CTLR, gicd_ctlr_val);
    gicv3_wait_for_rwp(GIC_SPI0_INTNO);

    // LOG_INFO("gicd_ctlr=0x%x\n", gicd_r(GICD_CTLR));
}

static void init_gicr_v3()
{
    int cpu_id = cpuid();

    /* LPI support is disabled */
    u32 gicr_ctlr_val = gicr_r32(cpu_id, GICR_CTLR);
    gicr_w32(cpu_id, GICR_CTLR, (gicr_ctlr_val & (~(0x1))));

    /* configure SGI/PPI to Non-secure Group 1
     * (group modifier bit: 0, group status bit: 1) */
    gicr_w32(cpu_id, GICR_IGROUPR0, ~0);
    gicr_w32(cpu_id, GICR_IGRPMODR0, 0);
    /*
     * When GICD_CTLR.DS==0:
     * GICR_IGRPMODR0 together with GICR_IGROUP0(group status bit),
     * control whether the corresponding interrupt is in:
     * - Secure Group 0.
     * - Non-secure Group 1.
     * - When System register access is enabled, Secure Group 1.
     */

    /* Enable Redistributor and wait GICR_WAKER's bit[2] become 0
     * which indicates that PE is active */
    u32 gicr_waker_val = gicr_r32(cpu_id, GICR_WAKER);
    gicr_w32(cpu_id, GICR_WAKER, (gicr_waker_val & (~GIC_GICR_WAKER_PS)));
    while (gicr_r32(cpu_id, GICR_WAKER) & GIC_GICR_WAKER_CA) {
        ;
    }
    isb();
}

static void init_gicc_v3()
{
    u64 sre_el1;
    u64 sre_el2;
    u64 icc_ctlr_el1_val;
    int cpu_id = cpuid();

#if GICR_NEED_INITIATE
    gicr_w32(cpu_id, GICR_IGROUPR0, ~((u32)(0)));      /* Configure SGIs/PPIs as non-secure Group-1 */
    gicr_w32(cpu_id, GICR_ICACTIVER0, ~((u32)(0)));    /* deactivate SGI/PPI interrupts */
    gicr_w32(cpu_id, GICR_ICENABLER0, 0xffff0000);     /* disable PPI interrupts(16~31) */
    gicr_w32(cpu_id, GICR_ICPENDR0, ~((u32)(0)));      /* clear SGI/PPI interrupts' pending state */
    for(int i = 0; i < GIC_SPI0_INTNO; i += GIC_GICD_IPRIORITY_PER_REG) {   /* setup SGI/PPI interrupts' priority to 0xA0 */
        gicr_w32(cpu_id, GICR_IPRIORITYR(i), GIC_INT_DEF_PRIO);
    }
    gicv3_wait_for_rwp(0);
    gicr_w32(cpu_id, GICR_ICFGR1, 0);  /* Configure PPIs as level triggered type */
#endif
    /* Disable Group0/Group1 interrupts */
    write_sysreg(icc_igrpen0_el1, 0);
    write_sysreg(icc_igrpen1_el1, 0);

    /* Enable SRE: The System register interface to the ICH_* registers and the EL1 and EL2 ICC_* registers is enabled for EL2.
     * EL1 accesses to ICC_SRE_EL1 do not trap to EL2. */

    read_sysreg(sre_el2, icc_sre_el2);
    sre_el2 |= (ICC_SRE_EL2_SRE | ICC_SRE_EL2_Enable);
    write_sysreg(icc_sre_el2, sre_el2);

    isb();

    /* Enable EL1 access cpu interface system register */
    read_sysreg(sre_el1, icc_sre_el1);
    sre_el1 |= ICC_SRE_EL1_SRE;
    write_sysreg(icc_sre_el1, sre_el1);

    /* allow all interrupts sent to PE */
    write_sysreg(icc_pmr_el1, GIC_IDLE_PRIO);

    /* set EOI mode to 1, write ICC_EOIR will only drop priority, won't deactivate irq */
    read_sysreg(icc_ctlr_el1_val, icc_ctlr_el1);
    write_sysreg(icc_ctlr_el1, (icc_ctlr_el1_val | ICC_CTLR_EOImode(1)));

    /* enable group1 interrupts */
    write_sysreg(icc_igrpen1_el1, 1);
    isb();
}

static u32 gic_v3_get_listregs()
{
    u32 tmp = 0;
    read_sysreg(tmp, ich_vtr_el2);
    return ((tmp & 0x1f) + 1);
}

static void init_gich_v3()
{
    /* enable virtual Group 1 interrupts */
    write_sysreg(ich_vmcr_el2, ICH_VMCR_VENG1);
    /* enable virtual CPU interface operation;
     * virtual CPU interface will be allowed to send virtual interrupts and maintenance interrupts */
    write_sysreg(ich_hcr_el2, ICH_HCR_EN);

    /* fetch the number of implemented List registers */
    g_gic_lr_max = gic_v3_get_listregs();
    isb();
}

void vm_gic_state_init(struct gic_state *gic_state)
{
    for (int i = 0; i < g_gic_lr_max; i++) {
        gic_state->lr[i] = 0;
    }
    read_sysreg(gic_state->vmcr_el2, ich_vmcr_el2);
}

void init_gicv3_percpu()
{
    init_gicc_v3();     /* GIC CPU interface */
    init_gicr_v3();     /* GIC Redistributor */

    init_gich_v3();     /* GIC Hypervisor control */
}

void gicv3_init()
{
    init_gicd_v3();
    init_gicv3_percpu();
}

enum gic_type {
    GIC_TYPE_GICD = 0,
    GIC_TYPE_GICR = 1,
};

static enum gic_type gic_base_type(u32 irq)
{
    if (irq < GIC_SPI0_INTNO) {
        return GIC_TYPE_GICR;
    } else {
        return GIC_TYPE_GICD;
    }
}

void gic_deactive_irq(u32 irq)
{
    write_sysreg(icc_dir_el1, irq);
}

static void gic_eoi(u32 irq, int group)
{
    if (group == 0) {
        write_sysreg(icc_eoir0_el1, irq);
    } else if (group == 1) {
        write_sysreg(icc_eoir1_el1, irq);
    } else {
        panic("[gic_eoi]: invalid group %d for irq %d\n", group, irq);
    }
}

void gic_host_eoi(u32 irq, int group)
{
    gic_eoi(irq, group);
    /* EOI mode is 1, need deactivate irq explicitly */
    gic_deactive_irq(irq);
}

void gic_guest_eoi(u32 irq, int group)
{
    /* priority drop */
    gic_eoi(irq, group);
}

void gic_set_target_by_affinity(u32 irq, u8 aff)
{
    /* TODO: 全局数组记录每个CPU的mpidr_el1, 然后用mpidr_el1中的aff0~aff3
     *       组合出aff变量写到GICD_IROUTER寄存器中 */
    int is_enable = gic_is_irq_enable(irq);
    if (is_enable) {
        gic_irq_disable(irq);
    }

    gicd_w(GICD_IROUTER(irq), aff);

    if (is_enable) {
        gic_irq_enable(irq);
    } else {
        gicv3_wait_for_rwp(irq);
    }
}

void gic_set_target_by_pe_field(u32 irq, u8 target)
{
    int is_enable = gic_is_irq_enable(irq);
    if (is_enable) {
        gic_irq_disable(irq);
    }

    u32 val = gicd_r(GICD_ITARGETSR(irq / 4));
    val &= ~((u32)0xff << (irq % 4 * 8));    // 将irq对应的GICD_ITARGETSR<n>中对应CPU_targets_offset字段清零
    LOG_INFO("setting val=%d\n", val | (target << (irq % 4 * 8)));
    gicd_w(GICD_ITARGETSR(irq / 4), val | (target << (irq % 4 * 8)));

    if (is_enable) {
        gic_irq_enable(irq);
    } else {
        gicv3_wait_for_rwp(irq);
    }
}

#if 0   /* 当affinity routing disable时, 才通过GICD_ITARGETSR<n>设置中断亲核性 */
void gic_set_target(u32 irq, u8 target)
{
    /* GICD_ITARGETSR<n> (n=0~254):
     * 31         ...         24 | 23         ...         16 | 15         ...         8 | 7         ...         0
     *   CPU_targets_offset_3B       CPU_targets_offset_2B       CPU_targets_offset_1B     CPU_targets_offset_0B
     */
    // LOG_INFO("[hyper] set irq(%d) to cpu(%d)\n", irq, target);

    u32 val = gicd_r(GICD_ITARGETSR(irq / 4));
    LOG_INFO("GICD_ITARGETSR=%p\n", val);
    val &= ~((u32)0xff << (irq % 4 * 8));    // 将irq对应的GICD_ITARGETSR<n>中对应CPU_targets_offset字段清零
    LOG_INFO("setting val=%d\n", val | (target << (irq % 4 * 8)));
    gicd_w(GICD_ITARGETSR(irq / 4), val | (target << (irq % 4 * 8)));

    LOG_INFO("gicd_itargetsr:%d\n", gicd_r(GICD_ITARGETSR(irq/4)));
}
#endif

void gic_irq_enable(u32 irq)
{
    enum gic_type base_type = gic_base_type(irq);
    u32 val = 0;

    if (base_type == GIC_TYPE_GICD) {
        val = gicd_r(GICD_ISENABLER(irq/32));
        LOG_INFO("[gic_irq_enable] before gicd_isenabler:%d\n", gicd_r(GICD_ISENABLER(irq/32)));
        val |= 1 << (irq % 32);
        gicd_w(GICD_ISENABLER(irq/32), val);
        LOG_INFO("[gic_irq_enable] gicd_isenabler:%d\n", gicd_r(GICD_ISENABLER(irq/32)));
    } else if (base_type == GIC_TYPE_GICR) {
        val = gicr_r32(cpuid(), GICR_ISENABLER0);
        val |= 1 << (irq % 32);
        gicr_w32(cpuid(), GICR_ISENABLER0, val);
    }
}

void gic_irq_disable(u32 irq)
{
    enum gic_type base_type = gic_base_type(irq);
    u32 val = 0;

    if (base_type == GIC_TYPE_GICD) {
        val = gicd_r(GICD_ISENABLER(irq/32));
        LOG_INFO("[gic_irq_disable] before gicd_isenabler:%d\n", gicd_r(GICD_ISENABLER(irq/32)));
        val &= ~(1 << (irq % 32));
        gicd_w(GICD_ISENABLER(irq/32), val);
        LOG_INFO("[gic_irq_disable] gicd_isenabler:%d\n", gicd_r(GICD_ISENABLER(irq/32)));
    } else if (base_type == GIC_TYPE_GICR) {
        val = gicr_r32(cpuid(), GICR_ISENABLER0);
        val &= ~(1 << (irq % 32));
        gicr_w32(cpuid(), GICR_ISENABLER0, val);
    }
}

int gic_is_irq_enable(u32 irq)
{
    int ret = 0;
    u32 val = 0;

    if (irq >= GIC_SPI0_INTNO && irq <= g_spi_num) {
        val = gicd_r(GICD_ISENABLER(irq/32));
        ret = val & (1 << (irq % 32));
    } else if (irq <= GIC_PPI_MAX) {
        val = gicr_r32(cpuid(), GICR_ISENABLER0);
        ret = val & (1 << irq);
    }

    return ret;
}

u32 gic_read_iar(void)
{
    u32 iar_el1 = 0;
    read_sysreg(iar_el1, icc_iar1_el1);
    return iar_el1;
}

int gic_max_spi(void)
{
    u32 gicd_typer = gicd_r(GICD_TYPER);
    u32 ITLinesNumber = (gicd_typer & 0x1f);
    int max_spi_intid = 32 * (ITLinesNumber + 1) - 1;
    
    /* interrupt IDs 1020-1023 are reserved for special purposes. */
    return max_spi_intid > 1019 ? 1019 : max_spi_intid;
}

bool gic_has_pending_lr(void)
{
    u64 lr = 0;
    
    for (int i = 0; i < g_gic_lr_max; i++) {
        lr = gic_read_lr(i);
    
        if (lr_is_pending(lr)) {
            return true;
        }
    }

    return false;
}

u64 gic_read_lr(int n)
{
#define READ_LR(n)  __READ_LR(n)
#define __READ_LR(n)  \
    case n: \
        read_sysreg(lr, ich_lr##n##_el2); \
        break;

    u64 lr = 0;
    switch (n) {
        READ_LR(0);
        READ_LR(1);
        READ_LR(2);
        READ_LR(3);
        READ_LR(4);
        READ_LR(5);
        READ_LR(6);
        READ_LR(7);
        READ_LR(8);
        READ_LR(9);
        READ_LR(10);
        READ_LR(11);
        READ_LR(12);
        READ_LR(13);
        READ_LR(14);
        READ_LR(15);
    }
    return lr;
}

void gic_write_lr(int n, u64 val)
{
    switch (n) {
    case 0:
        write_sysreg(ich_lr0_el2, val);
        break;
    case 1:
        write_sysreg(ich_lr1_el2, val);
        break;
    case 2:
        write_sysreg(ich_lr2_el2, val);
        break;
    case 3:
        write_sysreg(ich_lr3_el2, val);
        break;
    case 4:
        write_sysreg(ich_lr4_el2, val);
        break;
    case 5:
        write_sysreg(ich_lr5_el2, val);
        break;
    case 6:
        write_sysreg(ich_lr6_el2, val);
        break;
    case 7:
        write_sysreg(ich_lr7_el2, val);
        break;
    case 8:
        write_sysreg(ich_lr8_el2, val);
        break;
    case 9:
        write_sysreg(ich_lr9_el2, val);
        break;
    case 10:
        write_sysreg(ich_lr10_el2, val);
        break;
    case 11:
        write_sysreg(ich_lr11_el2, val);
        break;
    case 12:
        write_sysreg(ich_lr12_el2, val);
        break;
    case 13:
        write_sysreg(ich_lr13_el2, val);
        break;
    case 14:
        write_sysreg(ich_lr14_el2, val);
        break;
    case 15:
        write_sysreg(ich_lr15_el2, val);
        break;
    default:
        panic("[gic_write_lr]: invalid list register number %d\n", n);
    }
}

u64 gic_make_lr(u32 pirq, u32 virq, int group)
{
    return ICH_LR_STATE(LR_PENDING) | ICH_LR_HW | ICH_LR_GROUP(group) |
           ICH_LR_PINTID(pirq) | ICH_LR_VINTID(virq);
}

#define __fallthrough __attribute__((fallthrough))

static void gic_restore_lr(struct gic_state *gic)
{
    switch (g_gic_lr_max-1) {
    case 15:
        write_sysreg(ich_lr15_el2, gic->lr[15]);
        __fallthrough;
    case 14:
        write_sysreg(ich_lr14_el2, gic->lr[14]);
        __fallthrough;
    case 13:
        write_sysreg(ich_lr13_el2, gic->lr[13]);
        __fallthrough;
    case 12:
        write_sysreg(ich_lr12_el2, gic->lr[12]);
        __fallthrough;
    case 11:
        write_sysreg(ich_lr11_el2, gic->lr[11]);
        __fallthrough;
    case 10:
        write_sysreg(ich_lr10_el2, gic->lr[10]);
        __fallthrough;
    case 9:
        write_sysreg(ich_lr9_el2, gic->lr[9]);
        __fallthrough;
    case 8:
        write_sysreg(ich_lr8_el2, gic->lr[8]);
        __fallthrough;
    case 7:
        write_sysreg(ich_lr7_el2, gic->lr[7]);
        __fallthrough;
    case 6:
        write_sysreg(ich_lr6_el2, gic->lr[6]);
        __fallthrough;
    case 5:
        write_sysreg(ich_lr5_el2, gic->lr[5]);
        __fallthrough;
    case 4:
        write_sysreg(ich_lr4_el2, gic->lr[4]);
        __fallthrough;
    case 3:
        write_sysreg(ich_lr3_el2, gic->lr[3]);
        __fallthrough;
    case 2:
        write_sysreg(ich_lr2_el2, gic->lr[2]);
        __fallthrough;
    case 1:
        write_sysreg(ich_lr1_el2, gic->lr[1]);
        __fallthrough;
    case 0:
        write_sysreg(ich_lr0_el2, gic->lr[0]);
        __fallthrough;
    }
}

void gic_restore_state(struct gic_state *gic)
{
    u64 sre_el1 = 0;
    read_sysreg(sre_el1, icc_sre_el1);
    
    write_sysreg(ich_vmcr_el2, gic->vmcr_el2);
    write_sysreg(icc_sre_el1, (sre_el1 | gic->sre_el1));
    gic_restore_lr(gic);
}


void gic_set_pending_irq(u16 irq_id)
{
    gicd_w(GICD_ISPENDR((u32)irq_id / 32U), (1U << (irq_id % 32U)));
}