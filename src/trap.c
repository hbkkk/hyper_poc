#include "types.h"
#include "uart.h"
#include "aarch64.h"
#include "gic.h"
#include "timer.h"
#include "uart.h"
#include "vcpu.h"
#include "sysreg.h"
#include "mmu.h"
#include "mmio.h"
#include "smcc.h"
#include "psci.h"
#include "debug.h"

#define WFI_POLL_TIMEOUT_NS         (10000)     /* 0.01 ms */
#define WFI_POLL_TIMEOUT_NS_MAX     (500000)    /* 0.5 ms */

void el2_sync_handler(void)
{
    u64 far, elr, esr, spsr;
    read_sysreg(far, far_el2);
    read_sysreg(elr, elr_el2);
    read_sysreg(esr, esr_el2);
    read_sysreg(spsr, spsr_el2);
    LOG_ERR("el2_sync_handler: far_el2=0x%x, elr_el2=0x%x,"
            "esr_el2=0x%x, spsr_el2=0x%x\n", far, elr, esr, spsr);
    while (1)
        ;
}

#define PHYSICAL_TIMER_IRQ  30
#define VIRTUAL_TIMER_IRQ   27
#define UART_IRQ    33

void el2_irq_handler(void)
{
    u64 irq = 0;
    u64 far, elr, esr, spsr;
    read_sysreg(far, far_el2);
    read_sysreg(elr, elr_el2);
    read_sysreg(esr, esr_el2);
    read_sysreg(spsr, spsr_el2);
    LOG_TRACE("el2_irq_handler, far_el2=0x%x, elr_el2=0x%x,"
            "esr_el2=0x%x, spsr_el2=0x%x\n", far, elr, esr, spsr);

    read_sysreg(irq, icc_iar1_el1);
    LOG_TRACE("el2_irq_handler(%p), irq = %d\n", el2_irq_handler, irq);

    if (irq == UART_IRQ) {
        LOG_TRACE("Before clear uart interrupt, uart interrupt status: %d\n", uart_get_interrupt_status());
        clear_uart_interrupt();
        LOG_TRACE("After clear uart interrupt, uart interrupt status: %d\n", uart_get_interrupt_status());
    } else if (irq == PHYSICAL_TIMER_IRQ) {
        LOG_TRACE("disable phy timer\n");
        disable_timer();
        reload_timer();
        enable_timer();
        LOG_TRACE("reload timer done\n");
    }
    write_sysreg(icc_eoir1_el1, irq);
}

static void data_abort_iss_dump(u64 iss, u64 il)
{
    u32 iss_sas, iss_srt, iss_sf, iss_fnv, iss_wnr, iss_dfsc;

    iss_sas = (iss & DA_ISS_SAS_MASK) >> DA_ISS_SAS_OFFSET;
    iss_srt = (iss & DA_ISS_SRT_MASK) >> DA_ISS_SRT_OFFSET;
    iss_sf = (iss & DA_ISS_SF_MASK) >> DA_ISS_SF_OFFSET;
    iss_fnv = (iss & DA_ISS_FnV_MASK) >> DA_ISS_FnV_OFFSET;
    iss_wnr = (iss & DA_ISS_WnR_MASK) >> DA_ISS_WnR_OFFSET;
    iss_dfsc = (iss & DA_ISS_DFSC_MASK) >> DA_ISS_DFSC_OFFSET;

    LOG_TRACE("=======================================================\n"
              "data abort iss: 0x%x, Instruction Length: %s\n"
              "iss_sas = %d, iss_srt = %d, iss_sf = %d\n"
              "iss_fnv = %d, iss_wnr = %d, iss_dfsc = %d\n"
              "=======================================================\n",
              iss, il == 0 ? "16-bit" : "32-bit", iss_sas, iss_srt,
              iss_sf, iss_fnv, iss_wnr, iss_dfsc);
}

static int data_abort_handler(struct vcpu* vcpu, u64 esr)
{
    int ret = 0;
    u32 iss_srt, iss_fnv, iss_sas, iss_wnr;
    u64 iss, hpfar_el2, ipa, elr_el2, il, far_el2;
    (void)il;

    il = (esr & ESR_IL_MASK) >> ESR_IL_OFFSET;
    iss = (esr & ESR_ISS_MASK) >> ESR_ISS_OFFSET;

    // data_abort_iss_dump(iss, il);

    read_sysreg(far_el2, far_el2);
    read_sysreg(hpfar_el2, hpfar_el2);
    read_sysreg(elr_el2, elr_el2);
    
    iss_srt = (iss & DA_ISS_SRT_MASK) >> DA_ISS_SRT_OFFSET;
    iss_fnv = (iss & DA_ISS_FnV_MASK) >> DA_ISS_FnV_OFFSET;
    iss_sas = (iss & DA_ISS_SAS_MASK) >> DA_ISS_SAS_OFFSET;
    iss_wnr = (iss & DA_ISS_WnR_MASK) >> DA_ISS_WnR_OFFSET;

    if (iss_fnv) {
        LOG_ERR("Faulting instruction is not valid\n");
        return -1;
    }

    /* [NOTE]
     * IPA is composed of the contents in the two registers FAR_EL2 and HPFAR_EL2
     * HPFAR_EL2.FIPA: Faulting Intermediate Physical Address */
    ipa = ((hpfar_el2 & HPFAR_FIPA_MASK) << 8) | (far_el2 & (PAGE_SIZE-1));

    struct mmio_access mmio_access = {
        .ipa     = ipa,
        .pc      = elr_el2,
        .iss_sas = iss_sas,
        .iss_wnr = iss_wnr,
    };

    ret = mmio_emulate(vcpu, iss_srt, &mmio_access);
    if (ret < 0) {
        LOG_ERR("mmio_emulate failed with %d\n", ret);
        return -1;
    }

    vcpu->reg.elr_el2 += 4;
    return 0;
}

void advance_pc(struct vcpu *vcpu)
{
    vcpu->reg.elr_el2 += 4;
}

static int wfx_emulate_handler(struct vcpu *vcpu, u64 esr)
{
    u64 cur_time, end_time;
    end_time = count_to_time_ns(get_syscount()) + WFI_POLL_TIMEOUT_NS;
    do {
        if (gic_has_pending_lr() == true) {
            LOG_TRACE("[wfx_emulate_handler]: vcpu has pending lr\n");
            break;
        }
        cur_time = count_to_time_ns(get_syscount());
    } while (cur_time < end_time);

    advance_pc(vcpu);
    return 0;
}

static long standard_service_call(struct vcpu *vcpu)
{
    long ret = -1;
    unsigned long smc_fid = vcpu->reg.x[0];
    unsigned long x1 = vcpu->reg.x[1];
    unsigned long x2 = vcpu->reg.x[2];
    unsigned long x3 = vcpu->reg.x[3];

    if (is_psci_fid(smc_fid)) {
        ret = psci_handler(vcpu, (u32)smc_fid, x1, x2, x3);
    } else {
        panic("Unknown smc fid 0x%x\n", smc_fid);
    }

    return ret;
}

static int syscall_handler(struct vcpu *vcpu)
{
    unsigned long fid = vcpu->reg.x[0];

    long ret = -1;
    switch (fid & ~SMCC_FID_FN_NUM_MSK) {
        case SMCC64_FID_STD_SRVC:
            ret = standard_service_call(vcpu);
            break;
        default:
            panic("Unknown/Unsupported system monitor call fid 0x%x", fid);
    }

    vcpu->reg.x[0] = (unsigned long)ret;
    return ret;
}

static int hvc_handler(struct vcpu *vcpu, u64 esr)
{
    /* Note:
     * Different to smc call in vm, pc will advance 4 when vm invoke
     * hvc and trap to el2. So do not change elr_el2 here!
     */
    int ret = syscall_handler(vcpu);
    return ret;
}

static int smc_handler(struct vcpu *vcpu, u64 esr)
{
    int ret = syscall_handler(vcpu);

    /* Note:
     * pc won't advance 4 when vm invoke smc and trap to el2, so we
     * need to add 4 to variable elr_el2  */
    advance_pc(vcpu);

    return ret;
}

typedef int (*sync_trap_handler_t)(struct vcpu *vcpu, u64 esr);

sync_trap_handler_t get_sync_trap_handler(u64 esr)
{
    sync_trap_handler_t handler = NULL;
    u64 ec = (esr & ESR_EC_MASK) >> ESR_EC_OFFSET;

    switch (ec) {
        case ESR_EC_WFx:
            LOG_INFO("Trap guestos's wfi/wfe instruction.\n");
            handler = wfx_emulate_handler;
            break;
        case ESR_EC_HVC64:
            LOG_INFO("Guest OS invoke hvc instruction\n");
            handler = hvc_handler;
            break;
        case ESR_EC_SMC64:
            LOG_WARN("SMC64. (Not supported yet)\n");
            break;
        case ESR_EC_SYSRG:
            LOG_WARN("Trapped msr/mrs or system instruction. (Not supported yet)\n");
            break;
        case ESR_EC_IALEL:
            LOG_WARN("Instruction Abort from a lower Exception level. (Not supported yet)\n");
            break;
        case ESR_EC_PCALG:
            LOG_WARN("PC alignment fault exception. (Not supported yet)\n");
            break;
        case ESR_EC_DALEL:
            LOG_INFO("Data Abort from a lower Exception level.\n");
            handler = data_abort_handler;
            break;
        case ESR_EC_DAEL2:
            LOG_WARN("Data Abort from EL2. (Not supported yet)\n");
            break;
        case ESR_EC_SPALG:
            LOG_WARN("SP alignment fault exception. (Not supported yet)\n");
            break;
        default:
            LOG_ERR("Invalid exception class: 0x%x\n", ec);
            break;
    }

    return handler;
}


void lower_el_sync_handler(void)
{
    int ret = -1;
    u64 esr_el2 = 0;
    sync_trap_handler_t sync_trap_handler = NULL;
    struct vcpu *vcpu = cur_vcpu();
    
    read_sysreg(esr_el2, esr_el2);
    
#if 0
    LOG_TRACE("[lower_el_sync_handler]: ");
    vm_reg_dump();
#endif

    sync_trap_handler = get_sync_trap_handler(esr_el2);

    if (NULL != sync_trap_handler) {
        ret = sync_trap_handler(vcpu, esr_el2);
        if (ret != 0) {
            vm_reg_dump();
            panic("[lower_el_sync_handler]: sync trap handler failed with %d\n", ret);
        }
    } else {
        vm_reg_dump();
        panic("ERROR: invalid/unsupported exception class\n");
    }

    return;
}

void lower_el_irq_handler()
{
    u32 pirq = 0;
    u32 virq = 0;
    u32 iar = 0;
    u64 mpidr = 0;
    int group = 1;
    struct vcpu *vcpu = cur_vcpu();
    read_sysreg(mpidr, mpidr_el1);

    u32 time_pend = gicr_r32(0, GICR_ICPENDR0);
    u32 uart_pend = gicd_r(GICD_ICPENDR(UART_IRQ/32));
    // LOG_INFO("\n========= Enter [lower_el_irq_handler]: vcpu->cpuid=%d, pcpu=%d,"
    //        " time_pend:%p, uart_pend:%p\n",
    //        vcpu->cpuid, mpidr & 0xffffff, time_pend, uart_pend);
    // vm_reg_dump();
    // LOG_INFO("\n");

    vgic_used_lr_update(vcpu);

    iar = gic_read_iar();
    pirq = iar & 0xffffff;
    virq = pirq;

    /* TODO: check whether the coming irq belong to VM */

    gic_guest_eoi(pirq, group);

    vgic_inject_virq(vcpu, pirq, virq, group);

    isb();

    // LOG_INFO("========= Exit [lower_el_irq_handler]: vcpu->cpuid=%d, pcpu=%d\n",
    //        vcpu->cpuid, mpidr & 0xffffff);
}