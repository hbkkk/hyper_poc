#include "types.h"
#include "default_config.h"
#include "page_alloc.h"
#include "debug.h"

extern char __primary_stack_start[];
extern char __primary_stack_end[];

#define SP_MAGIC    (0xDEADBEEFDEAD0000)

u64 g_sp_bottom[PCPU_NUM_MAX] = {(u64)&__primary_stack_end};
u64 g_sp_top[PCPU_NUM_MAX];

void primary_stack_init(void)
{
    g_sp_top[0]     = (u64)&__primary_stack_start;
    g_sp_bottom[0]  = (u64)&__primary_stack_end;
    
    *(u64 *)g_sp_top[0] = SP_MAGIC | 0;
}

void secondary_stacks_init(void)
{
    u64 *p = NULL;
    for (int cpuid = 1; cpuid < PCPU_NUM; ++cpuid) {
        p = (u64*)alloc_page();
        if (NULL == p) {
            panic("[secondary_stacks_init]: no enough memory\n");
        }

        g_sp_top[cpuid]     = (u64)p;
        g_sp_bottom[cpuid]  = (u64)p + PAGE_SIZE;
        
        *(u64 *)g_sp_top[cpuid] = SP_MAGIC | cpuid;

        LOG_INFO("sp_top[%d]=0x%x, sp_bottom[%d]=0x%x, sp_top=0x%x\n",
               cpuid, g_sp_top[cpuid], cpuid, g_sp_bottom[cpuid],
               *(u64 *)g_sp_top[cpuid]);
    }
}

bool sp_canary_check(void)
{
    for (int cpuid = 0; cpuid < PCPU_NUM; ++cpuid) {
        if (*(u64 *)g_sp_top[cpuid] != (SP_MAGIC | cpuid)) {
            panic("[sp_canary_check]: CPU %d's el2 stack is collapsed (stack top: %x)\n",
                  cpuid, *(u64 *)g_sp_top[cpuid]);
        }
    }
    return true;
}