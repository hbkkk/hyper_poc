#include "types.h"
#include "vcpu.h"
#include "aarch64.h"
#include "debug.h"

extern u64 get_daif();
extern unsigned long get_current_el();
extern unsigned long get_vbar_el2();

u64 get_syscount(void)
{
    u64 cntpct_el0;
    read_sysreg(cntpct_el0, cntpct_el0);
    isb();
    return cntpct_el0;
}

void daif_info()
{
    u64 daif = get_daif();
    LOG_TRACE("D=%d, A=%d, I=%d, F=%d\n",
            (daif & (1<<9)) ? 1 : 0,
            (daif & (1<<8)) ? 1 : 0,
            (daif & (1<<7)) ? 1 : 0,
            (daif & (1<<6)) ? 1 : 0);
}

void record_system_registers(void)
{
    unsigned long cur_el = get_current_el();
    unsigned long os_vect = get_vbar_el2();

    LOG_INFO("os_vect: %p\n", os_vect);
    LOG_INFO("cur_el=%p\n", cur_el);

    // TODO: record system registers' origin value

    return;
}

void vm_reg_dump()
{
    struct vcpu *vcpu = cur_vcpu();
    u64 sp_el0, sp_el1, esr_el1, far_el1, par_el1, elr_el1, sctlr_el1;
    u64 hpfar_el2, far_el2, esr_el2;

    read_sysreg(sp_el0, sp_el0);
    read_sysreg(sp_el1, sp_el1);
    read_sysreg(elr_el1, elr_el1);
    read_sysreg(par_el1, par_el1);
    read_sysreg(sctlr_el1, sctlr_el1);
    read_sysreg(far_el1, far_el1);
    read_sysreg(far_el2, far_el2);
    read_sysreg(esr_el1, esr_el1);
    read_sysreg(esr_el2, esr_el2);
    read_sysreg(hpfar_el2, hpfar_el2);

    LOG_TRACE("vm=%s, vcpuid=%d\n", vcpu->vm->name, vcpu->cpuid);
    LOG_TRACE("  sp_el0   : %p\n", sp_el0);
    LOG_TRACE("  sp_el1   : %p\n", sp_el1);
    LOG_TRACE("  far_el1  : %p\n", far_el1);
    LOG_TRACE("  elr_el1  : %p\n", elr_el1);
    LOG_TRACE("  esr_el1  : %p\n", esr_el1);
    LOG_TRACE("  par_el1  : %p\n", par_el1);
    LOG_TRACE("  sctlr_el1: %p\n", sctlr_el1);
    LOG_TRACE("  far_el2  : %p\n", far_el2);
    LOG_TRACE("  esr_el2  : %p\n", esr_el2);
    LOG_TRACE("  elr_el2  : %p\n", vcpu->reg.elr_el2);
    LOG_TRACE("  spsr_el2 : %p\n", vcpu->reg.spsr_el2);
    LOG_TRACE("  hpfar_el2: %p\n", hpfar_el2);
    LOG_TRACE("  IPA      : %p\n", (((hpfar_el2 & 0xffffffffff0) << 8) | (far_el2 & 0xfff)));
    LOG_TRACE("  x30: %p\n",       vcpu->reg.x[30]);
    LOG_TRACE("  x29: %p\n",       vcpu->reg.x[29]);
    LOG_TRACE("  x28: %p\n",       vcpu->reg.x[28]);
    LOG_TRACE("  x27: %p\n",       vcpu->reg.x[27]);
    LOG_TRACE("  x26: %p\n",       vcpu->reg.x[26]);
    LOG_TRACE("  x25: %p\n",       vcpu->reg.x[25]);
    LOG_TRACE("  x24: %p\n",       vcpu->reg.x[24]);
    LOG_TRACE("  x23: %p\n",       vcpu->reg.x[23]);
    LOG_TRACE("  x22: %p\n",       vcpu->reg.x[22]);
    LOG_TRACE("  x21: %p\n",       vcpu->reg.x[21]);
    LOG_TRACE("  x20: %p\n",       vcpu->reg.x[20]);
    LOG_TRACE("  x19: %p\n",       vcpu->reg.x[19]);
    LOG_TRACE("  x18: %p\n",       vcpu->reg.x[18]);
    LOG_TRACE("  x17: %p\n",       vcpu->reg.x[17]);
    LOG_TRACE("  x16: %p\n",       vcpu->reg.x[16]);
    LOG_TRACE("  x15: %p\n",       vcpu->reg.x[15]);
    LOG_TRACE("  x14: %p\n",       vcpu->reg.x[14]);
    LOG_TRACE("  x13: %p\n",       vcpu->reg.x[13]);
    LOG_TRACE("  x12: %p\n",       vcpu->reg.x[12]);
    LOG_TRACE("  x11: %p\n",       vcpu->reg.x[11]);
    LOG_TRACE("  x10: %p\n",       vcpu->reg.x[10]);
    LOG_TRACE("  x09: %p\n",       vcpu->reg.x[9]);
    LOG_TRACE("  x08: %p\n",       vcpu->reg.x[8]);
    LOG_TRACE("  x07: %p\n",       vcpu->reg.x[7]);
    LOG_TRACE("  x06: %p\n",       vcpu->reg.x[6]);
    LOG_TRACE("  x05: %p\n",       vcpu->reg.x[5]);
    LOG_TRACE("  x04: %p\n",       vcpu->reg.x[4]);
    LOG_TRACE("  x03: %p\n",       vcpu->reg.x[3]);
    LOG_TRACE("  x02: %p\n",       vcpu->reg.x[2]);
    LOG_TRACE("  x01: %p\n",       vcpu->reg.x[1]);
    LOG_TRACE("  x00: %p\n",       vcpu->reg.x[0]);

}