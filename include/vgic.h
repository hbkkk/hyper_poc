#ifndef VGIC_H
#define VGIC_H

#include "gic.h"
#include "types.h"
#include "spinlock.h"

struct vcpu;
struct vm;

struct vgic_irq {
    u8 priority;  /* ipriorityr */
    u8 target;
    u8 enabled: 1;
    u8 igroup: 1;
    // TODO: u8 state; 维护VM中断的状态, 例如pending, active等
    // (可以参考bao-hypervisor的实现, vgic_irq中的state可以从lr寄存器中获取)
};

struct vgic {
    int             used;
    int             max_spi_intid;  /* Max SPI INTID */
    int             spi_nums;       /* Supported SPIs' number */
    bool            enable_grp1ns;  /* Enable Non-secure Group 1 interrupts */
    struct vgic_irq *spis;
    spinlock_t lock;
};

/* vgic cpu interface */
struct vgic_cpu {
    int used;
    u16 used_lr;
    struct vgic_irq sgis[GIC_NSGI];
    struct vgic_irq ppis[GIC_NPPI];
};

void vgic_used_lr_update(struct vcpu *vcpu);
struct vgic *new_vgic(struct vm *);
struct vgic_cpu *new_vgic_cpu(int vcpuid);
int vgic_inject_virq(struct vcpu *vcpu, u32 pirq, u32 virq, int group);
void vgic_restore_state(struct vgic_cpu *vgic);

void vgic_init(void);

#endif