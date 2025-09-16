#ifndef MMIO_H
#define MMIO_H

#include "types.h"
#include "vcpu.h"

struct vm;

enum syndrome_access_size {
    SAS_BYTE        = 0,
    SAS_HALFWORD    = 1,
    SAS_WORD        = 2,
    SAS_DOUBLEWORD  = 3,
};
  
struct mmio_access {
    u64                         ipa;        /* fault address of VM's IPA */
    u64                         pc;         /* PC where VM triggers data abort */
    enum syndrome_access_size   iss_sas;    /* size of the access attempted by the faulting operation */
    u32                         iss_wnr;    /* memory access's direction(write/read) which caused data abort */
};
  
struct mmio_info {
    struct mmio_info *next;
    u64 ipa_base;
    u64 size;
    int (*read)(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio);
    int (*write)(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio);
};

int mmio_emulate(struct vcpu *vcpu, int reg_idx, struct mmio_access *mmio_access);

int mmio_reg_handler(struct vm *vm, u64 ipa, u64 size,
                       int (*read)(struct vcpu *vcpu, u64 offset, u64 *val, struct mmio_access *mmio),
                       int (*write)(struct vcpu *vcpu, u64 offset, u64 val, struct mmio_access *mmio));

#endif