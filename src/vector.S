.section .text, "ax"

#define EXCEPTION_TABLE_ALIGN   (0x800)     /* because vbar_el2's [10:0] is RES0 */


.macro hyp_context_save
    stp x1, x0, [sp, #-16]!
    stp x3, x2, [sp, #-16]!
    stp x5, x4, [sp, #-16]!
    stp x7, x6, [sp, #-16]!
    stp x9, x8, [sp, #-16]!
    stp x11, x10, [sp, #-16]!
    stp x13, x12, [sp, #-16]!
    stp x15, x14, [sp, #-16]!
    stp x17, x16, [sp, #-16]!
    stp x19, x18, [sp, #-16]!
    stp x21, x20, [sp, #-16]!
    stp x23, x22, [sp, #-16]!
    stp x25, x24, [sp, #-16]!
    stp x27, x26, [sp, #-16]!
    stp x29, x28, [sp, #-16]!
    stp xzr, x30, [sp, #-16]!
.endm

.macro hyp_context_restore
    ldp xzr, x30, [sp], #16
    ldp x29, x28, [sp], #16
    ldp x27, x26, [sp], #16
    ldp x25, x24, [sp], #16
    ldp x23, x22, [sp], #16
    ldp x21, x20, [sp], #16
    ldp x19, x18, [sp], #16
    ldp x17, x16, [sp], #16
    ldp x15, x14, [sp], #16
    ldp x13, x12, [sp], #16
    ldp x11, x10, [sp], #16
    ldp x9, x8, [sp], #16
    ldp x7, x6, [sp], #16
    ldp x5, x4, [sp], #16
    ldp x3, x2, [sp], #16
    ldp x1, x0, [sp], #16
.endm


.macro vm_context_save
    stp x0, x1, [sp, #-16]!     /* stash x0,x1 on sp_el2 */
    mrs x0, tpidr_el2           /* x0 = &vcpu->reg */

    /* save gp and el2 sys reg into vcpu->reg */
    stp x2, x3, [x0, #16 * 1]
    stp x4, x5, [x0, #16 * 2]
    stp x6, x7, [x0, #16 * 3]
    stp x8, x9, [x0, #16 * 4]
    stp x10, x11, [x0, #16 * 5]
    stp x12, x13, [x0, #16 * 6]
    stp x14, x15, [x0, #16 * 7]
    stp x16, x17, [x0, #16 * 8]
    stp x18, x19, [x0, #16 * 9]
    stp x20, x21, [x0, #16 * 10]
    stp x22, x23, [x0, #16 * 11]
    stp x24, x25, [x0, #16 * 12]
    stp x26, x27, [x0, #16 * 13]
    stp x28, x29, [x0, #16 * 14]

    mrs x1, spsr_el2
    mrs x2, elr_el2
    stp x30, x1, [x0, #16 * 15]
    str x2, [x0, #16 * 16]

    ldp x3, x4, [sp], #16       /* stash pop x0, x1 then save into vcpu->reg */
    stp x3, x4, [x0, #16 * 0]
.endm


.macro vm_context_restore
    mrs x0, tpidr_el2           /* x0 = &vcpu->reg */

    ldp x30, x1, [x0, #16 * 15]  /* x1: spsr_el2 */
    ldr x2, [x0, #16 * 16]       /* x2: elr_el2 */
    
    ldp x3, x4, [x0, #16 * 0]    /* x3: x0, x4: x1 */
    stp x3, x4, [sp, #-16]!      /* stash x0,x1 on sp_el2 */

    msr spsr_el2, x1
    msr elr_el2, x2

    ldp x2, x3, [x0, #16 * 1]
    ldp x4, x5, [x0, #16 * 2]
    ldp x6, x7, [x0, #16 * 3]
    ldp x8, x9, [x0, #16 * 4]
    ldp x10, x11, [x0, #16 * 5]
    ldp x12, x13, [x0, #16 * 6]
    ldp x14, x15, [x0, #16 * 7]
    ldp x16, x17, [x0, #16 * 8]
    ldp x18, x19, [x0, #16 * 9]
    ldp x20, x21, [x0, #16 * 10]
    ldp x22, x23, [x0, #16 * 11]
    ldp x24, x25, [x0, #16 * 12]
    ldp x26, x27, [x0, #16 * 13]
    ldp x28, x29, [x0, #16 * 14]
    ldp x0, x1, [sp], #16        /* stash pop x0,x1 from sp_el2 */
.endm


.global eret_vm
eret_vm:
    vm_context_restore
    eret

.globl hyp_vector_table
.balign EXCEPTION_TABLE_ALIGN
hyp_vector_table:
    /*###########################
     *  Current EL with SP_EL0 
     *###########################*/
    b .                             // sync
    .balign 0x80
    b .                             // irq
    .balign 0x80
    b .                             // fiq
    .balign 0x80
    b .                             // error
    .balign 0x80

    /*###########################
     * Current EL with SP_ELx
     *###########################*/
    b el2_sync                      // sync
    .balign 0x80
    b el2_irq                       // irq
    .balign 0x80
    b .                             // fiq
    .balign 0x80
    b .                             // error
    .balign 0x80

    /*###########################
     *  Lower EL using aarch64
     *###########################*/
    b sync_exception_from_lower_el  // sync
    .balign 0x80
    b irq_exception_from_lower_el   // irq
    .balign 0x80
    b .                             // fiq
    .balign 0x80
    b .                             // error
    .balign 0x80

    /*###########################
     *  Lower EL using aarch32
     *###########################*/
    b .                             // sync
    .balign 0x80
    b .                             // irq
    .balign 0x80
    b .                             // fiq
    .balign 0x80
    b .                             // error
    .balign 0x80


el2_sync:
    hyp_context_save
    bl el2_sync_handler
    hyp_context_restore
    eret


el2_irq:
    hyp_context_save
    bl el2_irq_handler
    hyp_context_restore
    eret


sync_exception_from_lower_el:
    vm_context_save
    bl lower_el_sync_handler
    bl eret_vm


irq_exception_from_lower_el:
    vm_context_save
    bl lower_el_irq_handler
    bl eret_vm