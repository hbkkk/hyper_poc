#include "psci.h"
#include "debug.h"

extern void _start(void);

static long int psci_cpu_on(struct vcpu *vcpu, u64 x1, u64 x2, u64 x3)
{
    u64 target_cpu = x1;
    u64 entry_addr = x2;

    LOG_INFO("[psci_cpu_on]: Bring up cpu %d, entry_addr=%p\n", target_cpu, entry_addr);

    // TODO: need add vcpu to pcpu, if support more than one vm!
    struct vcpu *target = vcpu->vm->vcpus[target_cpu];

    target->reg.elr_el2 = entry_addr;

    vcpu_ready(target);

    return psci_call(PSCI_CPU_ON, target_cpu, (u64)_start, 0);
}

static long int psci_features_handler(u32 smc_fid)
{
    long int ret = PSCI_E_NOT_SUPPORTED;

    switch (smc_fid) {
        case PSCI_VERSION:
        case PSCI_CPU_OFF:
        case PSCI_CPU_SUSPEND_SMC32:
        case PSCI_CPU_SUSPEND_SMC64:
        case PSCI_CPU_ON_SMC32:
        case PSCI_CPU_ON_SMC64:
        case PSCI_AFFINITY_INFO_SMC32:
        case PSCI_AFFINITY_INFO_SMC64:
        case PSCI_FEATURES:
            ret = PSCI_E_SUCCESS;
            break;
        default:
            ret = PSCI_E_NOT_SUPPORTED;
            break;
    }

    return ret;
}

long psci_handler(struct vcpu *vcpu, u32 smc_fid, u64 x1, u64 x2, u64 x3)
{
    long int ret = -1;

    switch (smc_fid) {
        case PSCI_VERSION:
            ret = PSCI_VERSION_0_2;
            break;
        case PSCI_CPU_ON_SMC64:
            ret = psci_cpu_on(vcpu, x1, x2, x3);
            break;
        case PSCI_FEATURES:
            ret = psci_features_handler((u32)x1);
            break;
        default:
            panic("Unsupported smc_fid 0x%x\n", smc_fid);
    }
    
    return ret;
}