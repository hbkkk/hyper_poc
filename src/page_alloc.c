#include "page_alloc.h"
#include "lib.h"

#define BIT_PER_CHAR        (8)
#define BIT_PER_LONG        (sizeof(unsigned long) * BIT_PER_CHAR)
#define BITMAP_ENTRY(nr)    ((nr) / BIT_PER_LONG)
#define BITMAP_OFFSET(nr)   ((nr) % BIT_PER_LONG)

static inline int test_bit(const unsigned long *addr, unsigned long nr) {
    return (addr[BITMAP_ENTRY(nr)] >> BITMAP_OFFSET(nr)) & 1UL;
}

static inline void set_bit(unsigned long *addr, unsigned long nr) {
    addr[BITMAP_ENTRY(nr)] |= 1UL << BITMAP_OFFSET(nr);
}

static inline void clear_bit(unsigned long *addr, unsigned long nr) {
    addr[BITMAP_ENTRY(nr)] &= ~(1UL << BITMAP_OFFSET(nr));
}


static struct page_allocator g_allocator;

int page_allocator_init(unsigned long *bitmap_buf, unsigned long total_pages, unsigned long phyaddr)
{
    if (!bitmap_buf || total_pages == 0) {
        return -1;
    }
    pfn_t base_pfn = phy_to_pfn(phyaddr);
    
    g_allocator.bitmap = bitmap_buf;
    g_allocator.total_pages = total_pages;
    g_allocator.base_pfn = base_pfn;
    g_allocator.last_alloc = 0;
    spinlock_init(&g_allocator.lock);
    
    /* 初始化所有页为空闲状态 */
    memset(bitmap_buf, 0, (total_pages + BIT_PER_LONG - 1) / BIT_PER_LONG);
    return 0;
}


/**
 * __find_free_range - 在位图中查找连续空闲页
 * @nr_pages: 需要的连续页数
 *
 * 返回：找到的起始页号，失败返回 -1ULL
 */
static pfn_t __find_free_range(unsigned long nr_pages)
{
    pfn_t start = 0;
    unsigned long cnt = 0;
    unsigned long total = g_allocator.total_pages;

    /* 从最后分配位置开始搜索 */
    for (pfn_t i = g_allocator.last_alloc; i < total; i++) {
        if (!test_bit(g_allocator.bitmap, i)) {
            if (cnt == 0) {
                start = i;
            }
            if (++cnt >= nr_pages) {
                return start;
            }
        } else {
            cnt = 0;
        }
    }

    /* 回绕到起始位置继续搜索 */
    for (pfn_t i = 0; i < g_allocator.last_alloc; i++) {
        if (!test_bit(g_allocator.bitmap, i)) {
            if (cnt == 0) {
                start = i;
            }
            if (++cnt >= nr_pages) {
                return start;
            }
        } else {
            cnt = 0;
        }
    }

    return -1ULL;
}


unsigned long alloc_pages(unsigned long nr_pages)
{
    pfn_t start_page;

    if (nr_pages == 0 || nr_pages > g_allocator.total_pages) {
        return -1ULL;
    }

    spin_lock(&g_allocator.lock);
    
    start_page = __find_free_range(nr_pages);
    if (start_page == -1ULL) {
        spin_unlock(&g_allocator.lock);
        return -1ULL;
    }

    /* 标记页为已使用 */
    for (unsigned long i = start_page; i < start_page + nr_pages; i++) {
        set_bit(g_allocator.bitmap, i);
    }

    g_allocator.last_alloc = start_page + nr_pages;
    spin_unlock(&g_allocator.lock);

    return pfn_to_phy(g_allocator.base_pfn + start_page);
}


int free_pages(unsigned long phyaddr, unsigned long nr_pages)
{
    unsigned long start;
    pfn_t pfn = phy_to_pfn(phyaddr);

    if (pfn < g_allocator.base_pfn || nr_pages == 0) {
        return -1;
    }

    start = pfn - g_allocator.base_pfn;
    if (start + nr_pages > g_allocator.total_pages) {
        return -1;
    }

    spin_lock(&g_allocator.lock);

    for (unsigned long i = start; i < start + nr_pages; i++) {
        clear_bit(g_allocator.bitmap, i);
    }

    /* 优化下次搜索起始位置 */
    if (start < g_allocator.last_alloc) {
        g_allocator.last_alloc = start;
    }

    spin_unlock(&g_allocator.lock);
    return 0;
}

unsigned long alloc_page(void)
{
    return alloc_pages(1);
}

int free_page(unsigned long phyaddr)
{
    return free_pages(phyaddr, 1);
}