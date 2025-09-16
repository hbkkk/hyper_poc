#ifndef VCPU_H
#define VCPU_H

#include "types.h"
#include "vm.h"
#include "vgic.h"
#include "gic.h"
#include "aarch64.h"

enum vcpu_state {
    UNUSED,
    CREATED,
    READY,
    RUNNING,
};

struct cpu_features {
    u64 pfr0;
};

struct vcpu {
    struct {
        u64 x[31];
        u64 spsr_el2;
        u64 elr_el2;
    } reg;
    struct {
        u64 spsr_el1;
        u64 elr_el1;
        u64 mpidr_el1;
        u64 midr_el1;
        u64 sp_el0;
        u64 sp_el1;
        u64 ttbr0_el1;
        u64 ttbr1_el1;
        u64 tcr_el1;
        u64 vbar_el1;
        u64 sctlr_el1;
        u64 cntv_ctl_el0;
        u64 cntv_tval_el0;
        u64 cntfrq_el0;
    } sys;

    const char *name;

    enum vcpu_state state;

    struct cpu_features features;

    /* virtual interrupt related registers, includes: 
     * lr: inject virtual interrupt to VM
     * vmcr_el2: virtual machine control register
     * sre_el1: enable VM's access to GICC system register
     */
    struct gic_state gic;
    
    /* vgic_cpu records VM gicr/gicd's configuration which is managed by hypervisor.
     * Any VM's access to gicr/gicd MMIO region is intercepted by hypervisor due to
     * VM's gicd/gicr region lacking stage-2 translation.
     * Then hypervisor will return/save VM's gicd/gicr info, and access the real 
     * gicr/gicd MMIO region if necessary.
     */
    struct vgic_cpu *vgic;

    struct vm *vm;

    int cpuid;
};

struct vcpu *new_vcpu(struct vm *vm, int vcpuid, u64 entrypoint);
void free_vcpu(struct vcpu *vcpu);

void vcpu_ready(struct vcpu *vcpu);

void enter_vcpu(void);

void vcpu_init(void);

void vcpu_dump(struct vcpu *vcpu);

static inline bool vcpu_running(struct vcpu *vcpu) {
     return vcpu->state == RUNNING;
}

static inline struct vcpu *cur_vcpu() {
    struct vcpu *vcpu;
    read_sysreg(vcpu, tpidr_el2);
    return vcpu;
}

#endif