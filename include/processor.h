#ifndef HYPER_POC_GIC_H
#define HYPER_POC_GIC_H

static inline void cpu_relax(void)
{
    asm volatile("yield" ::: "memory");
}

static inline u8 cpu_cur_get(void)
{
    u32 arm_cpuid;

    asm volatile("mrs %0, tpidr_el2" : "=r"(arm_cpuid));

    return (u8)arm_cpuid;
}

#endif