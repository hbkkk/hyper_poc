#include "vm.h"
#include "lib.h"
#include "vcpu.h"
#include "vgic.h"
#include "mmio.h"
#include "guest.h"
#include "mmu.h"
#include "virtio.h"
#include "page_alloc.h"
#include "debug.h"

struct vm g_vms[VM_MAX];

static struct vm *allocvm() {
    for (int i = 0; i < VM_MAX; ++i) {
        if (g_vms[i].used == 0) {
            g_vms[i].used = 1;
            return &g_vms[i];
        }
    }
    return NULL;
}

void s2_pt_trap(struct vm *vm, u64 ipa, u64 size,
                int (*read_handler)(struct vcpu *, u64, u64 *, struct mmio_access *),
                int (*write_handler)(struct vcpu *, u64, u64, struct mmio_access *))
{
    u64 *stage2_pt = vm->stage2_pt;
    if (pagewalk(stage2_pt, ipa, 0) != NULL) {
        pageunmap(stage2_pt, ipa, size);
    }

    int ret = mmio_reg_handler(vm, ipa, size, read_handler, write_handler);
    if (ret < 0) {
        panic("mmio_reg_handler failed");
    }

    tlb_flush();
}

extern char _binary_guest_xv6_start[];
extern char _binary_guest_xv6_size[];
extern char _binary_guest_xv6_end[];

extern char _binary_guest_xv6_fs_img_start[];
extern char _binary_guest_xv6_fs_img_size[];
extern char _binary_guest_xv6_fs_img_end[];

void create_vm(struct vmconfig *vmcfg)
{
    LOG_INFO("_binary_guest_xv6_start: %p\n", _binary_guest_xv6_start);
    LOG_INFO("_binary_guest_xv6_size : %p\n", _binary_guest_xv6_size);
    LOG_INFO("_binary_guest_xv6_end  : %p\n", _binary_guest_xv6_end);

#if 0
    u64 *fs_ptr = (u64*)_binary_guest_xv6_fs_img_start;
    for (; fs_ptr < (u64 *)_binary_guest_xv6_fs_img_end; ++fs_ptr) {
        LOG_INFO("fs_ptr(%p): %p\n", ((u64)fs_ptr - (u64)_binary_guest_xv6_fs_img_start), *fs_ptr);
    }
    while (1)
        ;
#endif

    if (vmcfg == NULL || vmcfg->guest_img == NULL) {
        panic("vmcfg is NULL");
    }
    struct guest *guest_img = vmcfg->guest_img;

    struct vm *vm = allocvm();
    if (vm == NULL) {
        panic("alloc vm failed");
    }
    vm->nvcpu = vmcfg->nvcpu;
    strcpy(vm->name, guest_img->name);

    vm->vcpus[0] = new_vcpu(vm, 0, vmcfg->entrypoint);

    for (int i = 1; i < vmcfg->nvcpu; ++i) {
        vm->vcpus[i] = new_vcpu(vm, i, 0);
    }

    vm->stage2_pt = (u64 *)alloc_page();
    if (NULL == vm->stage2_pt) {
        panic("[create_vm] no mem");
    }

    u64 p, size, ipa, pa;

    /* NOTE:
     * 这里需要保证映射的IPA就是guest os编译时指定的链接地址区域,
     * elr_el2设置的值也就是这块IPA区域的首地址（即: vmcfg->entrypoint）,
     * 只有这样才能保证eret返回到VM运行时, pc指向到正确的IPA后, guestos才能正常运行起来！
     */

    u64 guest_filesz = guest_img->end - guest_img->start;
    u64 page_num = 0;

    LOG_INFO("\n=====================  READY TO COPY GUEST IMAGE and Map GuestOS's stage2 table  =====================>\n\n");
    LOG_INFO("guest_img at RAM pa=%p, guest_img->size(memsize)=%p, guest_img's filesize=%p\n\n",
            guest_img->start, guest_img->size, guest_filesz);

    /* create stage2 page table for guest os image */
    LOG_INFO("map guest_img's file size content:\n");
    for (p = 0; p < guest_filesz; p += PAGE_SIZE) {
        void *page = (void *)alloc_page();
        if (NULL == page) {
            panic("[create_vm] no mem 2");
        }
        ++page_num;

        if (guest_filesz - p < PAGE_SIZE) {
            size = guest_filesz - p;
        } else {
            size = PAGE_SIZE;
        }
        memcpy(page, (char *)guest_img->start + p, size);

        ipa = vmcfg->entrypoint + p;
        pa = (u64)page;
        LOG_INFO("--- IPA: %p, PA: %p\n", ipa, (u64)pa);
        pagemap(vm->stage2_pt, ipa, pa, PAGE_SIZE, S2PTE_NORMAL | S2PTE_RW);
    }
    LOG_INFO("\nmap remaining mem size content:\n");
    for (; p < guest_img->size; p += PAGE_SIZE) {
        void *page = (void *)alloc_page();
        if (NULL == page) {
            panic("[create_vm] no mem 3");
        }
        ++page_num;
        memset(page, 0, PAGE_SIZE);
        ipa = vmcfg->entrypoint + p;
        pa = (u64)page;
        LOG_INFO("--- IPA: %p, PA: %p\n", ipa, (u64)pa);
        pagemap(vm->stage2_pt, ipa, pa, PAGE_SIZE, S2PTE_NORMAL | S2PTE_RW);
    }
    LOG_INFO("guest image's total page num = %d (%d KB)\n", page_num, page_num*4);
    LOG_INFO("=====================================================================================\n\n");

    page_num = 0;
    LOG_INFO("\n=======================  Map Guest OS's RAM(size=%p)  ========================\n", vmcfg->ram_size);
    /* create stage2 page table for guest os's RAM */
    for (; p < vmcfg->ram_size; p += PAGE_SIZE) {
        void *page = (void *)alloc_page();
        if (NULL == page) {
            panic("[create_vm] no mem 3");
        }
        ++page_num;

        ipa = vmcfg->entrypoint + p;
        pa = (u64)page;
        // LOG_TRACE("--- IPA: %p, PA: %p\n", ipa, (u64)pa);
        pagemap(vm->stage2_pt, ipa, pa, PAGE_SIZE, S2PTE_NORMAL | S2PTE_RW);
    }
    LOG_INFO("ram's total page num = %d (%dKB, %dMB)\n", page_num, page_num*4, page_num*4/1024);
    LOG_INFO("=====================================================================================\n\n");

    LOG_INFO("\n======================  Map Guest OS's Uart(pa=ipa=%p)  ======================>\n\n", UARTBASE);
    /* create stage2 page table for guestos's peripheral */
    pagemap(vm->stage2_pt, UARTBASE, UARTBASE,PAGE_SIZE, S2PTE_DEVICE|S2PTE_RW);

    virtio_mmio_init(vm);

    vm->vgic = new_vgic(vm);

    vcpu_ready(vm->vcpus[0]);
}