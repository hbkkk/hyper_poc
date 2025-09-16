#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define CNTFRQ_MASK         (0xFFFFFFFFUL)

#define NS_PER_SECOND		(1000000000U)
#define NS_PER_MS			(1000000UL)
#define NS_PER_US			(1000U)

void freq_init(void);
void setup_timer(void);
void reload_timer(void);
void enable_timer(void);
void disable_timer();

u64 nstime_to_count(u64 nstime);
u64 count_to_time_ns(u64 count);

#endif