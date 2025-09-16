#include "mmu.h"
#include "page_alloc.h"
#include "lib.h"
#include "aarch64.h"
#include "debug.h"

u64 *pagewalk(u64 *pgt, u64 va, int need_alloc) {
    for(int level = 0; level < 3; level++) {
        u64 *pte = &pgt[PIDX(level, va)];
  
        if ( (*pte & PTE_VALID) && (*pte & PTE_TABLE) ) {
            pgt = (u64 *)PTE_PA(*pte);
        } else if (need_alloc) {
            pgt = (u64 *)alloc_page();
            if (!pgt)
                panic("nomem");
  
            *pte = PTE_PA(pgt) | PTE_TABLE | PTE_VALID;
        } else {
            /* pte already beed unmapped */
            return NULL;
        }
    }  
    return &pgt[PIDX(3, va)];
}
  
void pagemap(u64 *pgt, u64 va, u64 pa, u64 size, u64 attr) {
    if (va % PAGE_SIZE != 0 || pa % PAGE_SIZE != 0 || size % PAGE_SIZE != 0) {
        panic("invalid pagemap");
    }

    // LOG_INFO("[pagemap]: va = %p, pa = %p, size = %p\n", va, pa, size);
  
    for(u64 p = 0; p < size; p += PAGE_SIZE, va += PAGE_SIZE, pa += PAGE_SIZE) {
        u64 *pte = pagewalk(pgt, va, 1);
        if(*pte & PTE_AF) {
            LOG_ERR("*pte = %p\n", *pte);
            panic("[pagemap]: this entry has been used");
        }
  
        *pte = PTE_PA(pa) | S2PTE_AF | attr | PTE_V;
    }
}
  
void pageunmap(u64 *pgt, u64 va, u64 size) {
    if(va % PAGE_SIZE != 0 || size % PAGE_SIZE != 0)
        panic("invalid pageunmap");
  
    for(u64 p = 0; p < size; p += PAGE_SIZE, va += PAGE_SIZE) {
        u64 *pte = pagewalk(pgt, va, 0);
        if(*pte == 0)
            panic("unmapped");
  
        u64 pa = PTE_PA(*pte);
        free_page(pa);
        *pte = 0;
    }
}

u64 ipa2pa(u64 *pgt, u64 ipa)
{
    if (NULL == pgt) {
        return 0;
    }
    u64 *pte = pagewalk(pgt, ipa, 0);
    if(!pte) {
        return 0;
    }
    u32 off = ipa & (PAGE_SIZE - 1);
  
    return PTE_PA(*pte) + off;
}
  
void dump_par_el1(u64 par)
{
    if(par & 1) {
        LOG_WARN("translation fault\n");
        LOG_WARN("FST : %p\n", (par >> 1) & 0x3f);
        LOG_WARN("PTW : %p\n", (par >> 8) & 1);
        LOG_WARN("S   : %p\n", (par >> 9) & 1);
    } else {
        LOG_WARN("address: %p\n", par);
    }
} 
  
void stage2_mmu_init(void)
{
    u64 mmf;
    read_sysreg(mmf, id_aa64mmfr0_el1);
    LOG_INFO("id_aa64mmfr0_el1.parange = %p\n", mmf & 0xf);

    u64 vtcr = VTCR_T0SZ(20) | VTCR_SH0(0) | VTCR_SL0(2) |
               VTCR_TG0(0) | VTCR_NSW | VTCR_NSA | VTCR_PS(4);
    write_sysreg(vtcr_el2, vtcr);
    LOG_INFO("vtcr = %p\n", vtcr);

    /* 设置两种内存属性（DEVICE_nGnRnE、NORMAL_NC）到mair_el2中 */
    u64 mair = (AI_DEVICE_nGnRnE << (8 * AI_DEVICE_nGnRnE_IDX)) | (AI_NORMAL_NC << (8 * AI_NORMAL_NC_IDX));
    write_sysreg(mair_el2, mair);
  
    isb();
}
