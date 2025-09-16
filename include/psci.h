#ifndef PSCI_H
#define PSCI_H

#include "types.h"
#include "vcpu.h"

#define PSCI_VERSION             (0x84000000)
#define PSCI_CPU_SUSPEND_SMC32   (0x84000001)
#define PSCI_CPU_SUSPEND_SMC64   (0xc4000001)
#define PSCI_CPU_OFF             (0x84000002)
#define PSCI_CPU_ON_SMC32        (0x84000003)
#define PSCI_CPU_ON_SMC64        (0xc4000003)
#define PSCI_AFFINITY_INFO_SMC32 (0x84000004)
#define PSCI_AFFINITY_INFO_SMC64 (0xc4000004)
#define PSCI_FEATURES            (0x8400000A)
#define PSCI_MIG_INFO_TYPE       (0x84000006)

#ifdef AARCH32
#define PSCI_CPU_SUSPEND   PSCI_CPU_SUSPEND_SMC32
#define PSCI_CPU_ON        PSCI_CPU_ON_SMC32
#define PSCI_AFFINITY_INFO PSCI_AFFINITY_INFO_SMC32
#else
#define PSCI_CPU_SUSPEND   PSCI_CPU_SUSPEND_SMC64
#define PSCI_CPU_ON        PSCI_CPU_ON_SMC64
#define PSCI_AFFINITY_INFO PSCI_AFFINITY_INFO_SMC64
#endif

#define PSCI_VERSION_0_2               (2U)

#define PSCI_E_SUCCESS                 0
#define PSCI_E_NOT_SUPPORTED           -1
#define PSCI_E_INVALID_PARAMS          -2
#define PSCI_E_DENIED                  -3
#define PSCI_E_ALREADY_ON              -4
#define PSCI_E_ON_PENDING              -5
#define PSCI_E_INTERN_FAIL             -6
#define PSCI_E_NOT_PRESENT             -7
#define PSCI_E_DISABLED                -8
#define PSCI_E_INVALID_ADDRESS         -9

/* The macros below are used to identify PSCI calls from the SMC function ID */
#define SMC_FID_MASK                   (0xff000000)

#define SMC32_STDSRVC_FID_VALUE        (0x84000000)
#define is_smc32_stdsrvc_fid(_fid)     (((_fid) & SMC_FID_MASK) == SMC32_STDSRVC_FID_VALUE)

#define SMC64_STDSRVC_FID_VALUE        (0xc4000000)
#define is_smc64_stdsrvc_fid(_fid)     (((_fid) & SMC_FID_MASK) == SMC64_STDSRVC_FID_VALUE)

#define is_smc_stdsrvc_fid(_fid)       (is_smc64_stdsrvc_fid(_fid) || is_smc32_stdsrvc_fid(_fid))

#define PSCI_FID_MASK                  (0xffe0)
#define PSCI_FID_VALUE                 (00)
#define is_psci_fid(_fid)              (is_smc_stdsrvc_fid(_fid) && (((_fid) & PSCI_FID_MASK) == PSCI_FID_VALUE))

long psci_handler(struct vcpu *vcpu, u32 smc_fid, u64 x1, u64 x2, u64 x3);

u64 psci_call(u32 func, u64 cpuid, u64 entry, u64 ctxid);

#endif