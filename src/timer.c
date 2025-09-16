#include "timer.h"
#include "aarch64.h"
#include "gic.h"
#include "types.h"
#include "debug.h"

#define PHYSICAL_TIMER_IRQ  30
#define VIRTUAL_TIMER_IRQ   27

#define CNTV_CTL_ENABLE   (1<<0)
#define CNTV_CTL_IMASK    (1<<1)
#define CNTV_CTL_ISTATUS  (1<<2)

static u64 g_freq;

static u64 do_muldiv64(u64 a, u32 b, u32 c)
{
	union {
		u64 ll;
		struct {
			u32 low, high;
		} l;
	} u, res;
	u64 rl, rh;

	u.ll = a;
	rl = (u64)u.l.low * (u64)b;
	rh = (u64)u.l.high * (u64)b;
	rh += (rl >> 32);

	res.l.high = (u32)(rh / c);

	rh = (rh % c) << 32;
	rl = rl & 0xffffffffUL;
	res.l.low = (u32)((rh + rl) / c);

	return res.ll;
}

/** 
 * @brief: do the caculation abort (a*b)/c 
 */
static u64 muldiv64(u64 a, u32 b, u32 c)
{
	u64 ret;

	if (c != 0U) {
		ret = do_muldiv64(a, b, c);
	} else {
		LOG_WARN("[muldiv64]: Warning c=0, return 0\n");
		ret = 0UL;
	}

	return ret;
}

/** 
 * @brief: Convert nanosecond time to system count
 */
u64 nstime_to_count(u64 nstime)
{
	return muldiv64(nstime, g_freq, NS_PER_SECOND);
}

/** 
 * @brief: Convert system count to nanosecond time
 */
u64 count_to_time_ns(u64 count)
{
	return muldiv64(count, NS_PER_SECOND, g_freq);
}

void enable_timer() {
  	u64 c;
  	read_sysreg(c, cntp_ctl_el0);
  	c |= CNTV_CTL_ENABLE;
  	c &= ~CNTV_CTL_IMASK;
  	write_sysreg(cntp_ctl_el0, c);
}

void disable_timer() {
    u64 c;
  	read_sysreg(c, cntp_ctl_el0);
  	c &= ~CNTV_CTL_ENABLE;
  	c |= CNTV_CTL_IMASK;
  	write_sysreg(cntp_ctl_el0, c);
}

void reload_timer() {
    write_sysreg(cntp_tval_el0, g_freq/10000);    // 0.1毫秒 (100微秒)
    // write_sysreg(cntp_tval_el0, g_freq/100);
}

// Use physical timer instead of virtual timer
void setup_timer()
{
    // enable tick irq
    gic_irq_enable(PHYSICAL_TIMER_IRQ);
    // gic_irq_enable(VIRTUAL_TIMER_IRQ);

    read_sysreg(g_freq, cntfrq_el0);
    LOG_TRACE("[setup_timer]: g_freq=%u\n", g_freq);
    disable_timer();
    reload_timer();
    enable_timer();
}

void freq_init(void)
{
    read_sysreg(g_freq, cntfrq_el0);
    g_freq = g_freq & CNTFRQ_MASK;
}