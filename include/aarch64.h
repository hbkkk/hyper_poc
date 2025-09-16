#ifndef HYPER_POC_AARCH64_H
#define HYPER_POC_AARCH64_H

#include "types.h"

#define arm_sysreg(op1, crn, crm, op2)  \
  s3_ ## op1 ## _ ## crn ## _ ## crm ## _ ## op2

#define __read_sysreg(val, reg) \
  asm volatile("mrs %0, " #reg : "=r"(val))
#define read_sysreg(val, reg)  __read_sysreg(val, reg)

#define __write_sysreg(reg, val)  \
  asm volatile("msr " #reg ", %0" : : "r"(val))
#define write_sysreg(reg, val)  \
  do { u64 x = (u64)(val); __write_sysreg(reg, x); } while(0)

#define intr_enable()   asm volatile("msr daifclr, #2" ::: "memory")
#define intr_disable()  asm volatile("msr daifset, #2" ::: "memory")

#define isb()     asm volatile("isb" ::: "memory");
#define dsb(ty)   asm volatile("dsb " #ty);

#define HCR_VM    (1 << 0)
#define HCR_SWIO  (1 << 1)
#define HCR_FMO   (1 << 3)
#define HCR_IMO   (1 << 4)
#define HCR_TWI   (1 << 13)
#define HCR_TWE   (1 << 14)
#define HCR_TID3  (1 << 18)
#define HCR_TSC   (1 << 19)
#define HCR_TGE   (1 << 27)
#define HCR_RW    (1 << 31)

#define HPFAR_FIPA_MASK   (0xffffffffff0)

#define SPSR_EL2_MODE_EL1   (0b0100)
#define SPSR_EL2_MODE_EL0T  (0x0 << 0)
#define SPSR_EL2_MODE_EL1T  (0x4 << 0)
#define SPSR_EL2_MODE_EL1H  (0x5 << 0)
#define SPSR_EL2_F          (1 << 6)
#define SPSR_EL2_I          (1 << 7)
#define SPSR_EL2_A          (1 << 8)
#define SPSR_EL2_D          (1 << 9)

#define MPIDR_RES1          (1 << 31)

#define ESR_EC_OFFSET       (26)
#define ESR_EC_MASK         (0x3f << ESR_EC_OFFSET)
#define ESR_IL_OFFSET       (25)
#define ESR_IL_MASK         (1 << ESR_IL_OFFSET)
#define ESR_ISS_OFFSET      (0)
#define ESR_ISS_MASK        (0x1ffffff << ESR_ISS_OFFSET)


#define DA_ISS_SAS_MASK     (0x3 << 22)
#define DA_ISS_SAS_OFFSET   (22)
#define DA_ISS_SRT_MASK     (0x1f << 16)
#define DA_ISS_SRT_OFFSET   (16)
#define DA_ISS_SF_MASK      (1 << 15)
#define DA_ISS_SF_OFFSET    (15)
#define DA_ISS_FnV_MASK     (1 << 10)
#define DA_ISS_FnV_OFFSET   (10)
#define DA_ISS_WnR_MASK     (1 << 6)
#define DA_ISS_WnR_OFFSET   (6)
#define DA_ISS_DFSC_MASK    (0x3f << 0)
#define DA_ISS_DFSC_OFFSET  (0)

#define ISS_SAS_BYTE        (0b00)
#define ISS_SAS_HALFWORD    (0b01)
#define ISS_SAS_WORD        (0b10)
#define ISS_SAS_DOUBLEWORD  (0b11)

#define ISS_WnR_READ        (0b0)
#define ISS_WnR_WRITE       (0b1)

#define ESR_EC_WFx          (0b000001)  // 0x1
#define ESR_EC_HVC64        (0b010110)  // 0x16
#define ESR_EC_SMC64        (0b010111)  // 0x17
#define ESR_EC_SYSRG        (0b011000)  // 0x18
#define ESR_EC_IALEL        (0b100000)  // 0x20
#define ESR_EC_PCALG        (0b100010)  // 0x22
#define ESR_EC_DALEL        (0b100100)  // 0x24
#define ESR_EC_DAEL2        (0b100101)  // 0x25
#define ESR_EC_SPALG        (0b100110)  // 0x26

#define SCTLR_EL1_M         (0b01)

// 这种方法应该仅在qemu上有效, 对于板子上的mpidr的值, 可能低4位均为0
// TODO: 修改cpuid的实现方式
static inline int cpuid() {
    int mpidr;
    read_sysreg(mpidr, mpidr_el1);
    return mpidr & 0xf;
}

static inline u64 vttbr_ipa2pa(u64 ipa) {
    u64 par;
    asm volatile("at s12e1r, %0" :: "r"(ipa) : "memory");
    read_sysreg(par, par_el1);
    return par;
}

static inline void tlb_flush() {
    dsb(ishst);
    asm volatile("tlbi vmalls12e1");
    dsb(ish);
    isb();
}

static inline u64 vm_va_to_ipa(u64 va, bool is_el0) {
    u64 par;
    if (is_el0) {
        asm volatile("at s1e0r, %0" :: "r"(va)); // EL0 转换
    } else {
        asm volatile("at s1e1r, %0" :: "r"(va)); // EL1 转换
    }
    asm volatile("mrs %0, par_el1" : "=r"(par)); // 读取结果
    if (par & 0x1) {
        return ~0UL; // 转换失败
    }
    return (par & 0x000FFFFFFFFFF000) | (va & 0xFFF); // IPA = PAR[51:12] + VA[11:0]
}

static inline u64 vm_va_to_pa(u64 va, bool is_el0) {
    u64 par, orig_par;

    read_sysreg(orig_par, par_el1);
    if (is_el0) {
        asm volatile("at s12e0r, %0" :: "r"(va)); // EL0 + Stage-2
    } else {
        asm volatile("at s12e1r, %0" :: "r"(va)); // EL1 + Stage-2
    }
    read_sysreg(par, par_el1);
    write_sysreg(par_el1, orig_par);    // restore PAR_EL1
    if (par & 0x1) {
        return ~0UL; // address translate failed
    }
    return (par & 0x000FFFFFFFFFF000) | (va & 0xFFF); // PA = PAR[51:12] + VA[11:0]
}

bool is_pa_valid(u64 pa);

#endif
