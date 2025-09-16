#include "calltrace.h"
#include "default_config.h"
#include "aarch64.h"
#include "debug.h"
#include "vcpu.h"

extern u64 g_sp_bottom[PCPU_NUM_MAX];
extern u64 g_sp_top[PCPU_NUM_MAX];

static void __dump_hyper_calltrace(u64 fp, u64 lr)
{
    int cpu_id = cpuid();
    u64 fp_tmp, lr_tmp;
    u64 stack_bottom = g_sp_bottom[cpu_id];
    u64 stack_top = g_sp_top[cpu_id];

    if (stack_top == 0 || stack_bottom == 0) {
        panic("[dump_hyper_calltrace]: invalid stack(cpuid=%d, top=%p, bottom=%p)\n",
              cpu_id, stack_top, stack_bottom);
    }

    fp_tmp = fp;
    lr_tmp = lr;

    printf("=============== Hypervisor Calltrace on cpu[%d]: ===============\n", cpu_id);

    do {
        lr_tmp = *(u64 *)(fp_tmp + sizeof(u64));
        lr_tmp -= 4;
        printf("%s addr:%p, fp:%p\n", "-", lr_tmp, fp_tmp);
        fp_tmp = *(u64*)fp_tmp;
        if ((fp_tmp > stack_bottom) || (fp_tmp < stack_top)) {
            break;
        }
    } while (1);
    printf("\n");

    return;
}

void dump_hyper_calltrace(void) {
    unsigned long fp, lr;

    asm volatile("str x29, %0 \n" : "=m" (fp) : : "memory");
    asm volatile("str x30, %0 \n" : "=m" (lr) : : "memory");

    __dump_hyper_calltrace(fp, lr);
}



/* [ VM的calltrace打印实现思考 ]
 *
 * 难点1: 如何知道是从GuestOS用户态、还是GuestOS内核态陷入的EL2?
 * 办法: 通过spsr_el2.M[3:0]字段可以判断出是从EL0、还是EL1陷入的EL2！
 *
 * 难点2: 如何获取到VM用户态、内核态栈空间的地址范围？
 * 思考: 感觉好像在Hypervisor中是感知不到以上信息的, 要么有一种取巧的办法,
 *       固定就打印3、4个调用栈, 就算越界了也没事
 * 
 * 难点3: 如果是从VM的用户态陷入EL2, sp_el0一定指向用户态的栈指针, 可以根据fp、lr来尝试
 *        有限次数的栈回溯. 但是若是从VM的内核态陷入的EL2, 那么就无法得到用户态的fp、lr，
 *        VM用户态的fp、lr在陷入VM内核态时都保存在内核task_struct中了, 该信息对于Hypervisor
 *        而言是透明的，它不得而知！
 * 
 * 难点4: VM是否开启MMU也是需要考虑的！
 * 
 * 
 * 因此:
 * - VM's EL0 trap into EL2：可以尝试打印VM用户态calltrace
 * - VM's EL1 trap into EL2：可以尝试打印VM内核态calltrace, 无法打印用户态calltrace
 */
void dump_vm_calltrace(void)
{
    u64 fp_tmp, lr_tmp, sctlr_el1, ttbr0_el1, ttbr1_el1, pa_addr;
    bool is_el0 = false;
    bool trap_from_el1 = false;
    bool vm_enable_mmu = false;
    struct vcpu *vcpu = cur_vcpu();
    if (vcpu->state != RUNNING) {
        return;
    }
    
    trap_from_el1 = !!((vcpu->reg.spsr_el2) & SPSR_EL2_MODE_EL1);
    is_el0 = !trap_from_el1;

    read_sysreg(sctlr_el1, sctlr_el1);
    vm_enable_mmu = sctlr_el1 & SCTLR_EL1_M;

    if (vm_enable_mmu) {
        read_sysreg(ttbr0_el1, ttbr0_el1);
        read_sysreg(ttbr1_el1, ttbr1_el1);
    }

    fp_tmp = vcpu->reg.x[29];
    lr_tmp = vcpu->reg.x[30];

    printf("=============== VM(%s) EL%s Calltrace on cpu[%d]: ===============\n",
           vcpu->vm->name, trap_from_el1 ? "1" : "0", cpuid());
    printf("%s elr :%p\n", "-", vcpu->reg.elr_el2);
    // printf("%s addr:%p, fp:%p\n", "-", lr_tmp, fp_tmp);
    for (int i = 0; i < 5; ++i)
    {
#if 0
        printf("fp's va  =%p\n"
               "fp's ipa =%p\n"
               "fp's pa  =%p (by ats12)\n"
               "fp's pa  =%p (by pagewalk)\n",
               fp_tmp, vm_va_to_ipa(fp_tmp, is_el0), vm_va_to_pa(fp_tmp, is_el0),
               ipa2pa(vcpu->vm->stage2_pt, vm_va_to_ipa(fp_tmp, is_el0)));
#endif
        /* Note: 这里的fp地址是VM的VA/IPA，需要先将其转为IPA，再由IPA转为PA，然后读取上面的内容 */
        if (vm_enable_mmu) {
            /* VM fp's VA -> PA */
            pa_addr = vm_va_to_pa(fp_tmp + sizeof(u64), is_el0);
        } else {
            /* VM fp's IPA -> PA */
            pa_addr = ipa2pa(vcpu->vm->stage2_pt, fp_tmp + sizeof(u64));
        }
        if (is_pa_valid(pa_addr) == false)
            break;

        lr_tmp = *(u64 *)(pa_addr);
        lr_tmp -= 4;
        printf("%s addr:%p, fp:%p\n", "-", lr_tmp, fp_tmp);

        if (vm_enable_mmu) {
            pa_addr = vm_va_to_pa(fp_tmp, is_el0);
        } else {
            pa_addr = ipa2pa(vcpu->vm->stage2_pt, fp_tmp);
        }
        if (is_pa_valid(pa_addr) == false)
            break;

        fp_tmp = *(u64*)pa_addr;
    }
    printf("\n");

    return;
}