#ifndef DEFAULT_CONFIG_H
#define DEFAULT_CONFIG_H

#define PCPU_NUM        SMP_NUM

#define VCPU_MAX        4
#define NCPU            4
#define VM_MAX          2

#define PCPU_NUM_MAX     32

#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif

#endif