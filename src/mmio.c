#include "spinlock.h"
#include "log.h"
#include "vcpu.h"
#include "vm.h"
#include "mmio.h"
#include "debug.h"

#define MMIO_INFO_MAX  64

struct mmio_info g_mmio_info[MMIO_INFO_MAX];
spinlock_t g_mmio_lock = SPINLOCK_INITVAL;

static struct mmio_info *alloc_mmio_info(struct mmio_info *prev)
{
    spin_lock(&g_mmio_lock);
    for (int i = 0; i < MMIO_INFO_MAX; i++) {
        if (g_mmio_info[i].size == 0) {
            g_mmio_info[i].size = 1;
            g_mmio_info[i].next = prev;
            spin_unlock(&g_mmio_lock);
            return &g_mmio_info[i];
        }
    }
    spin_unlock(&g_mmio_lock);
    return NULL;
}

int mmio_emulate(struct vcpu *vcpu, int reg_idx, struct mmio_access *mmio_access)
{
    struct mmio_info *mmio = vcpu->vm->mmio_list;
    if (NULL == mmio) {
        LOG_INFO("[mmio_emulate]: mmio list of vcpu(id=%d, vm=%s) is empty\n",
               vcpu->cpuid, vcpu->vm->name);
        return -1;
    }

    u64 ipa = mmio_access->ipa;
    u64 *reg = NULL;
    u64 val = 0;

    if (reg_idx == 31) {
        val = 0;
    } else {
        reg = &vcpu->reg.x[reg_idx];
        val = *reg;
    }

    while (mmio) {
        if (mmio->ipa_base <= ipa && ipa < mmio->ipa_base+mmio->size) {
            if (mmio_access->iss_wnr && NULL != mmio->write) {
                return mmio->write(vcpu, ipa - mmio->ipa_base, val, mmio_access);
            } else if (!mmio_access->iss_wnr && NULL != mmio->read) {
                return mmio->read(vcpu, ipa - mmio->ipa_base, reg, mmio_access);
            } else {
                LOG_WARN("[mmio_emulate]: invalid\n");
                return -1;
            }
        }
        mmio = mmio->next;
    }

    LOG_WARN("[mmio_emulate]: there is no mmio region that can match ipa(%x)\n", ipa);
    return -1;
}

int mmio_reg_handler(struct vm *vm, u64 ipa, u64 size,
    int (*read)(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio),
    int (*write)(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio))
{
    if (NULL == vm || size <= 0) {
        return -1;
    }

    struct mmio_info *mmio_new = alloc_mmio_info(vm->mmio_list);
    if (NULL == mmio_new) {
        return -1;
    }
    vm->mmio_list = mmio_new;

    mmio_new->ipa_base = ipa;
    mmio_new->size = size;
    mmio_new->read = read;
    mmio_new->write = write;

    return 0;
}