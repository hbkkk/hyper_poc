#ifndef VM_H
#define VM_H

#include "types.h"
#include "spinlock.h"
#include "guest.h"
#include "default_config.h"

struct mmio_access;
struct mmio_info;
struct vcpu;

struct vmconfig {
    struct guest  *guest_img;
    struct guest  *fdt_img;
    struct guest  *initrd_img;
    int           nvcpu;
    u64           ram_size;     /* guest可用RAM大小(单位: 字节), 包含了guest img的大小 */
    
    /* guestos编译的ld脚本中指定的entrypoint地址, 该地址也是guest os
     * 所在的IPA内存区域的首地址; vm's elr_el2寄存器中设置的就是该值！
     * Hypervisor为guest os image建立stage2页表映射时, IPA用的就是该变量！
     */
    u64           entrypoint;   
};

struct vm {
    char              name[64];
    int               nvcpu;
    struct vcpu       *vcpus[VCPU_MAX];
    u64               *stage2_pt;
    struct vgic       *vgic;
    struct mmio_info  *mmio_list;
    int               used;
    u64               fdt;    /* fdt base address for linux */
};

void s2_pt_trap(struct vm *vm, u64 ipa, u64 size,
                int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
                int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *));

void create_vm(struct vmconfig *vmcfg);

#endif