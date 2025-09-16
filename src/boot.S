.section .text.boot, "ax"

.macro set_sp cpu_id
    msr spsel, #1
    ldr x0, =g_sp_bottom
    ldr x0, [x0, \cpu_id, uxtw #3]
    mov sp, x0
.endm

.global _start
_start:
    // ldr x8, =0x9000000
    // mov x4, 'G'
    // str x4, [x8]

    mrs x1, mpidr_el1
    and x1, x1, #0xff       /* 获取当前CPU号, Aff0 = mpidr_el1[7:0] */
    msr tpidr_el2, x1

    set_sp  w1

    ldr x5, =hyp_vector_table
    msr vbar_el2, x5
    isb

    cbz x1, vmm_init_primary
    bl vmm_init_secondary

    b .

.global get_vbar_el2
get_vbar_el2:
    mrs x0, vbar_el2
    ret

.global primary_cpuid_set
primary_cpuid_set:
    mov   x18, #0
    msr tpidr_el2, x18
    ret

.global get_current_el
get_current_el:
    mrs x0, CurrentEL
    ret

.global get_daif
get_daif:
    mrs x0, DAIF
    ret

.global psci_call
psci_call:
    smc #0
    ret