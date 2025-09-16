#include "vcpu.h"
#include "vm.h"
#include "aarch64.h"
#include "spinlock.h"
#include "debug.h"

static struct vcpu vcpus[VCPU_MAX];
static spinlock_t g_vcpus_lock;

void eret_vm(void);
static void save_sysreg(struct vcpu *vcpu);
static void restore_sysreg(struct vcpu *vcpu);

void vcpu_init(void)
{
    spinlock_init(&g_vcpus_lock);
}

static struct vcpu *vcpu_alloc() {
    spin_lock(&g_vcpus_lock);
    for (int i = 0; i < VCPU_MAX; i++) {
        if(vcpus[i].state == UNUSED) {
            vcpus[i].state = CREATED;
            spin_unlock(&g_vcpus_lock);
            return &vcpus[i];
        }
    }
    spin_unlock(&g_vcpus_lock);
  
    return NULL;
}

struct vcpu *new_vcpu(struct vm *vm, int vcpuid, u64 entrypoint)
{
    struct vcpu *vcpu = vcpu_alloc();
    if (!vcpu) {
        return NULL;
    }

    vcpu->vm = vm;
    vcpu->cpuid = vcpuid;
    vcpu->name = "vcpu";

    vcpu->reg.elr_el2 = entrypoint;
    vcpu->reg.spsr_el2 = SPSR_EL2_MODE_EL1H | SPSR_EL2_F | SPSR_EL2_I | SPSR_EL2_A | SPSR_EL2_D;

    vcpu->sys.mpidr_el1  = vcpuid | MPIDR_RES1;
    vcpu->sys.midr_el1   = 0x410fd081; // Cortex-A72
    vcpu->sys.sctlr_el1  = 0x30C50830;
    vcpu->sys.cntfrq_el0 = 62500000;

    /* TODO: init fdt address for guest os */

    /* Initialize the virtual machine view of the GIC state */
    vm_gic_state_init(&vcpu->gic);

    /* Initialize virtual interrupt related registers */
    vcpu->vgic = new_vgic_cpu(vcpu->cpuid);

    return vcpu;
}

void free_vcpu(struct vcpu *vcpu)
{

}

void vcpu_ready(struct vcpu *vcpu)
{
    vcpu->state = READY;
}

static void switch_to_vcpu(struct vcpu *vcpu)
{
    write_sysreg(tpidr_el2, vcpu);

    if (cpuid() != vcpu->cpuid) {
        panic("vcpu cpuid mismatch");
    }

    vcpu->state = RUNNING;

    write_sysreg(vttbr_el2, vcpu->vm->stage2_pt);
    tlb_flush();

    restore_sysreg(vcpu);
    gic_restore_state(&vcpu->gic);
    isb();

#if DEBUG_MODE
    vcpu_dump(vcpu);
#endif

    eret_vm();
}

void enter_vcpu()
{
    int id = cpuid();
    struct vcpu *vcpu = &vcpus[id];

    if (vcpu->state != READY) {
        panic("vcpu not ready");
    }

    LOG_TRACE("enter vm(%s)'s vcpu %d\n", vcpu->vm->name, id);

    switch_to_vcpu(vcpu);
}

static void save_sysreg(struct vcpu *vcpu)
{
    read_sysreg(vcpu->sys.spsr_el1, spsr_el1);
    read_sysreg(vcpu->sys.elr_el1, elr_el1);
    read_sysreg(vcpu->sys.mpidr_el1, mpidr_el1);
    read_sysreg(vcpu->sys.midr_el1, midr_el1);
    read_sysreg(vcpu->sys.sp_el0, sp_el0);
    read_sysreg(vcpu->sys.sp_el1, sp_el1);
    read_sysreg(vcpu->sys.ttbr0_el1, ttbr0_el1);
    read_sysreg(vcpu->sys.ttbr1_el1, ttbr1_el1);
    read_sysreg(vcpu->sys.tcr_el1, tcr_el1);
    read_sysreg(vcpu->sys.vbar_el1, vbar_el1);
    read_sysreg(vcpu->sys.sctlr_el1, sctlr_el1);
    read_sysreg(vcpu->sys.cntv_ctl_el0, cntv_ctl_el0);
    read_sysreg(vcpu->sys.cntv_tval_el0, cntv_tval_el0);
}

/* restore_sysreg will only be invoked when entering the VM on the first time */
static void restore_sysreg(struct vcpu *vcpu)
{
    write_sysreg(spsr_el1, vcpu->sys.spsr_el1);
    write_sysreg(elr_el1, vcpu->sys.elr_el1);
    
    /* Emulate the MPIDR_EL1 and MIDR_EL1 register */
    write_sysreg(vmpidr_el2, vcpu->sys.mpidr_el1);
    write_sysreg(vpidr_el2, vcpu->sys.midr_el1);

    write_sysreg(sp_el0, vcpu->sys.sp_el0);
    write_sysreg(sp_el1, vcpu->sys.sp_el1);
    write_sysreg(ttbr0_el1, vcpu->sys.ttbr0_el1);
    write_sysreg(ttbr1_el1, vcpu->sys.ttbr1_el1);
    write_sysreg(tcr_el1, vcpu->sys.tcr_el1);
    write_sysreg(vbar_el1, vcpu->sys.vbar_el1);
    write_sysreg(sctlr_el1, vcpu->sys.sctlr_el1);
    write_sysreg(cntv_ctl_el0, vcpu->sys.cntv_ctl_el0);
    write_sysreg(cntv_tval_el0, vcpu->sys.cntv_tval_el0);
    write_sysreg(cntfrq_el0, vcpu->sys.cntfrq_el0);
}

void vcpu_dump(struct vcpu *vcpu)
{
    if(!vcpu)
        return;

    save_sysreg(vcpu);

    LOG_TRACE("vcpu register dump vcpu[%d]\n", vcpu->cpuid);
    for(int i = 0; i < 31; i++) {
        LOG_TRACE("x%-2d %18p  ", i, vcpu->reg.x[i]);
    if((i+1) % 4 == 0)
        LOG_TRACE("\n");
    }
    LOG_TRACE("\n");
    LOG_TRACE("spsr_el2  %18p  elr_el2   %18p\n", vcpu->reg.spsr_el2, vcpu->reg.elr_el2);
    LOG_TRACE("spsr_el1  %18p  elr_el1   %18p  mpdir_el1    %18p\n",
           vcpu->sys.spsr_el1, vcpu->sys.elr_el1, vcpu->sys.mpidr_el1);
    LOG_TRACE("midr_el1  %18p  sp_el0    %18p  sp_el1       %18p\n",
           vcpu->sys.midr_el1, vcpu->sys.sp_el0, vcpu->sys.sp_el1);
    LOG_TRACE("ttbr0_el1 %18p  ttbr1_el1 %18p  tcr_el1      %18p\n",
           vcpu->sys.ttbr0_el1, vcpu->sys.ttbr1_el1, vcpu->sys.tcr_el1);
    LOG_TRACE("vbar_el1  %18p  sctlr_el1 %18p  cntv_ctl_el0 %18p\n",
           vcpu->sys.vbar_el1, vcpu->sys.sctlr_el1, vcpu->sys.cntv_ctl_el0);
}