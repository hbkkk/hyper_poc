#include "uart.h"
#include "gic.h"
#include "types.h"
#include "processor.h"
#include "aarch64.h"
#include "timer.h"
#include "sysreg.h"
#include "vm.h"
#include "page_alloc.h"
#include "mmu.h"
#include "vgic.h"
#include "vcpu.h"
#include "guest.h"
#include "ramdisk.h"
#include "debug.h"

void hyp_vector_table();

#define BIT_PER_LONG    (sizeof(unsigned long) * 8)
#define RAM_PAGE_NUM    (RAM_SIZE / 4096)
#define BITMAP_SIZE     ((RAM_PAGE_NUM + BIT_PER_LONG - 1) / BIT_PER_LONG)

#define TEST_EL2_SYNC_EXCEPTION 0
#define UART_IRQ    33

extern char ram_start[];
extern char txt_start[];

unsigned long g_bitmap[BITMAP_SIZE] __attribute__((aligned(16)));

/* 每个CPU的EL2 hypervisor栈空间 */
__attribute__((aligned(16))) char g_cpu_stack[8192*NCPU] = {0};

extern void primary_cpuid_set();
extern void primary_stack_init(void);
extern void secondary_stacks_init(void);


bool is_pa_valid(u64 pa)
{
    return ( (pa >= (u64)txt_start) && (pa < ((u64)ram_start + RAM_SIZE)) );
}


static void test_el2_sync_exception()
{
    LOG_INFO("ready to trigger a el2 exception\n");
    unsigned long *tmp1 = (unsigned long *)0x800000000000/*NULL*/;  /* access NULL will not trigger el2 sync exception! WHY ??? */
    *tmp1 = 1;       // <<< will trigger el2 sync exception
}

// extern struct guest guest_hello[];
extern struct guest guest_xv6[];

struct vmconfig xv6_vmcfg = {
    // .guest_img = &guest_hello[GUEST_IMAGE],
    .guest_img = &guest_xv6[GUEST_IMAGE],
    .fdt_img = NULL,
    .initrd_img = NULL,
    .nvcpu = 4,
    .ram_size = 128*1024*1024,  /* 128M; Same with PHYSTOP in xv6 memlayout.h */
    .entrypoint = 0x40000000,   /* xv6's beginning phys addr, same with xv6's kernel.ld */
};

void enable_uart_irq_el2()
{
    gic_set_target_by_affinity(UART_IRQ, 0);
    gic_irq_enable(UART_IRQ);
}

static void hcr_setup() {
    /*【配置HCR_EL2寄存器】
     * - HCR_VM: 置1时, 使能EL0/EL1的stage2页表映射 
     * - HCR_IMO: 置1时, 中断路由到EL2、使能virtual interrupt
     * - HCR_RW: 置1时, EL1的执行状态是AArch64
     * - HCR_SWIO: 置1时, 存储操作会被视为具有 Write-Invalidate（写失效） 属性，
     *           即绕过缓存并失效（Invalidate）缓存中的对应数据.
     *           使 Guest OS 对 Device-nGnRE/Normal NC 的 STR 操作绕过缓存并失效缓存行.
     *           置0时, GuestOS str操作数据写入设备寄存器，但不会影响缓存
     * [为什么需要SWIO？]
     * Guest OS 可能不知道自己在虚拟化环境下运行，并假设 Device-nGnRE 存储操作会立即到达设备;
     * 但 Stage 2 页表 可能会引入额外的缓存行为（如 Normal Cacheable 映射）;
     * 如果 Hypervisor 不干预，Guest OS 的 STR 操作可能会被缓存，导致设备访问延迟或数据不一致;
     * 
     * - HCR_TSC: 置1时, EL1执行的smc指令, 会被路由到EL2（esr_el2.ec为0x17）;
     * - HCR_TWI: 置1时, EL0/EL1执行的wfi指令, 会被路由到EL2; 置0时, EL0/EL1执行wfi指令不会路由到EL2
     * - HCR_TWE: 置1时, EL0/EL1执行的wfe指令, 会被路由到EL2; 置0时, EL0/EL1执行wfe指令不会路由到EL2
     */
    u64 hcr = HCR_VM | HCR_SWIO | HCR_IMO | HCR_RW | HCR_TSC | HCR_TWI | HCR_TWE;

    write_sysreg(hcr_el2, hcr);
    isb();
}

extern char __primary_stack_start[];
extern char __primary_stack_end[];

extern u64 g_sp_bottom[PCPU_NUM_MAX];
extern u64 g_sp_top[PCPU_NUM_MAX];

int vmm_init_primary(void)
{
    primary_stack_init();
    uart_init();
    
    primary_cpuid_set();

    page_allocator_init(g_bitmap, RAM_PAGE_NUM, (unsigned long)ram_start);
    
    record_system_registers();

    LOG_TRACE("=====================  BASIC memeroy information  =====================\n"
           " - Total ram size : %p(%dMB)\n"
           " - RAM_PAGE_NUM   : %p\n"
           " - BITMAP_SIZE    : %p\n"
           " - ram start addr : %p\n"
           " - txt_start addr : %p\n",
           RAM_SIZE, RAM_SIZE/1024/1024, RAM_PAGE_NUM, BITMAP_SIZE, ram_start, txt_start);
    LOG_TRACE("=======================================================================\n");

    secondary_stacks_init();

    LOG_INFO("__primary_core_stack_start:%x, __primary_core_stack_end:%x\n",
             &__primary_stack_start, __primary_stack_end);
    for (int i = 0; i < PCPU_NUM; ++i) {
        LOG_INFO("CPU[%d]: sp_top=%p, sp_bottom=%p\n", i, g_sp_top[i], g_sp_bottom[i]);
    }

    gicv3_init();

    vgic_init();

    vcpu_init();

    freq_init();

    // setup_timer();  /* EL2下的timer还有些问题待调试, tick中断来了之后reload后下次再也进不去irq handler了 */

    enable_uart_irq_el2();

    hcr_setup();

    stage2_mmu_init();

    ramdisk_init();

    create_vm(&xv6_vmcfg);

    enter_vcpu();

    /****************** Following is self debug ******************/
    daif_info();

    intr_enable();

    daif_info();

    // test_el2_sync_exception();
    while (1) {
        u32 time_pend = gicr_r32(0, GICR_ICPENDR0);
        u32 uart_pend = gicd_r(GICD_ICPENDR(UART_IRQ/32));
        (void)uart_pend;
        (void)time_pend;
        LOG_TRACE("time_pend:%p, uart_pend:%p\n", time_pend, uart_pend);
    }

    LOG_TRACE("Out of while\n");

    unsigned long pc = 0;
    asm volatile("adr %0, ." : "=r" (pc));
    LOG_TRACE("pc=%p\n", pc);

    return -1;
}

int vmm_init_secondary(void)
{
    // TODO: 根据mpidr设置从核的cpuid
    u64 sp;
    asm volatile("mov %0, sp" : "=r"(sp));
    LOG_INFO("[vmm_init_secondary]: cpu=%d sp: 0x%x\n", cpuid(), sp);

    uart_init();
    write_sysreg(vbar_el2, (u64)hyp_vector_table);

    init_gicv3_percpu();

    hcr_setup();

    stage2_mmu_init();

    enter_vcpu();

    panic("[vmm_init_secondary]: should not be here!\n");
    return -1;
}
