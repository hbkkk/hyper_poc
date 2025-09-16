#include "spinlock.h"

typedef unsigned long pfn_t;

#ifndef PAGE_SHIFT
#define PAGE_SHIFT  (12)
#endif
#define pfn_to_phy(pfn)     ((pfn) << PAGE_SHIFT)
#define phy_to_pfn(phy)     ((phy) >> PAGE_SHIFT)

/* 页分配器结构体 */
struct page_allocator {
    spinlock_t lock;           // 保护结构的自旋锁
    unsigned long *bitmap;     // 位图指针
    unsigned long total_pages;        // 管理的总页数
    pfn_t base_pfn;           // 起始页帧号
    pfn_t last_alloc;         // 最后分配位置（优化搜索）
};

/**
 * page_allocator_init - 初始化页分配器
 * @bitmap_buf: 位图缓冲区指针
 * @total_pages: 管理的总页数
 * @phyaddr: 起始物理页地址
 *
 * 返回：0 成功，-1 失败
 */
int page_allocator_init(unsigned long *bitmap_buf, unsigned long total_pages,
                        unsigned long phyaddr);

unsigned long alloc_pages(unsigned long nr_pages);

int free_pages(unsigned long phyaddr, unsigned long nr_pages);

unsigned long alloc_page(void);

int free_page(unsigned long phyaddr);