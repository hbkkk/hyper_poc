#ifndef SYSREG_H
#define SYSREG_H

u64 get_syscount(void);
void daif_info(void);
void record_system_registers(void);
void vm_reg_dump(void);

#endif