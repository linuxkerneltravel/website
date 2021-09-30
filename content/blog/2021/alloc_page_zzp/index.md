---
title: "alloc_page分配内存"
date: 2021-09-28T11:02:11+08:00
author: "张子攀"
keywords: ["内存分配","伙伴算法"]
categories : ["内存管理"]
banner : "img/blogimg/1.png"
summary : "在内核初始化完成之后, 内存管理的责任就由伙伴系统来承担。Linux内核使用二进制伙伴算法来管理和分配物理内存页面。伙伴系统是一个结合了2的方幂个分配器和空闲缓冲区合并计技术的内存分配方案, 其基本思想很简单。内存被分成含有很多页面的大块, 每一块都是2个页面大小的方幂. 如果找不到想要的块, 一个大块会被分成两部分, 这两部分彼此就成为伙伴。其中一半被用来分配, 而另一半则空闲。这些块在以后分配的过程中会继续被二分直至产生一个所需大小的块。当一个块被最终释放时, 其伙伴将被检测出来, 如果伙伴也空闲则合并两者."
---

# alloc_page分配内存

在内核初始化完成之后, 内存管理的责任就由伙伴系统来承担。Linux内核使用二进制伙伴算法来管理和分配物理内存页面, 该算法由Knowlton设计, 后来Knuth又进行了更深刻的描述。伙伴系统是一个结合了2的方幂个分配器和空闲缓冲区合并计技术的内存分配方案, 其基本思想很简单. 内存被分成含有很多页面的大块, 每一块都是2个页面大小的方幂. 如果找不到想要的块, 一个大块会被分成两部分, 这两部分彼此就成为伙伴. 其中一半被用来分配, 而另一半则空闲. 这些块在以后分配的过程中会继续被二分直至产生一个所需大小的块. 当一个块被最终释放时, 其伙伴将被检测出来, 如果伙伴也空闲则合并两者。

内存分配函数及功能如下：
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210712211658657.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80OTI5ODkzMQ==,size_16,color_FFFFFF,t_70)

通过使用标志、内存域修饰符和各个分配函数，内核提供了一种非常灵活的内存分配体系。尽管如此, 所有接口函数都可以追溯到一个简单的基本函数(alloc_pages_node)。

内存分配API统一到alloc_pages接口
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210712211513452.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80OTI5ODkzMQ==,size_16,color_FFFFFF,t_70)

分配单页的函数alloc_page和__get_free_page, 还有__get_dma_pages借助于宏定义。

```c
#define alloc_page(gfp_mask) alloc_pages(gfp_mask, 0)


#define __get_free_page(gfp_mask) \
    __get_free_pages((gfp_mask), 0)`


#define __get_dma_pages(gfp_mask, order) \
    __get_free_pages((gfp_mask) | GFP_DMA, (order))
```

get_zeroed_page的实现,是对__get_free_pages使用__GFP_ZERO标志，即可分配填充字节0的页. 再返回与页关联的内存区地址即可.

```c
unsigned long get_zeroed_page(gfp_t gfp_mask)
{
        return __get_free_pages(gfp_mask | __GFP_ZERO, 0);
}
EXPORT_SYMBOL(get_zeroed_page);
```

__get_free_pages调用alloc_pages完成内存分配, 而alloc_pages又借助于alloc_pages_node，__get_free_pages函数的定义在mm/page_alloc.c

```c
unsigned long __get_free_pages(gfp_t gfp_mask, unsigned int order)
{
    struct page *page;

    /*
     * __get_free_pages() returns a 32-bit address, which cannot represent
     * a highmem page
     */
    VM_BUG_ON((gfp_mask & __GFP_HIGHMEM) != 0);

    page = alloc_pages(gfp_mask, order);
    if (!page)
        return 0;
    return (unsigned long) page_address(page);
}
EXPORT_SYMBOL(__get_free_pages);
```

## alloc_pages函数分配页

alloc_pages函数调用流程如下：



![在这里插入图片描述](https://img-blog.csdnimg.cn/20210712214031822.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80OTI5ODkzMQ==,size_16,color_FFFFFF,t_70)

**alloc_pages**函数定义如下：

```c
#ifdef CONFIG_NUMA

static inline struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
        return alloc_pages_current(gfp_mask, order);
}

#else

#define alloc_pages(gfp_mask, order) \
                alloc_pages_node(numa_node_id(), gfp_mask, order)
#endif
```

**alloc_pages**是通过alloc_pages_node函数实现的, alloc_pages_node函数定义：

```c
/*
 * Allocate pages, preferring the node given as nid. When nid == NUMA_NO_NODE,
 * prefer the current CPU's closest node. Otherwise node must be valid and
 * online.
 */
static inline struct page *alloc_pages_node(int nid, gfp_t gfp_mask,
                        unsigned int order)
{
    if (nid == NUMA_NO_NODE)
        nid = numa_mem_id();

    return __alloc_pages_node(nid, gfp_mask, order);
}
```

它执行了一个简单的检查, 如果指定负的结点ID(不存在, 即NUMA_NO_NODE = -1), 内核自动地使用当前执行CPU对应的结点nid = numa_mem_id();, 然后调用__alloc_pages_node函数进行了内存分配

**__alloc_pages_node函数：**

```c
/*
 * Allocate pages, preferring the node given as nid. The node must be valid and
 * online. For more general interface, see alloc_pages_node().
 */
static inline struct page *
__alloc_pages_node(int nid, gfp_t gfp_mask, unsigned int order)
{
    VM_BUG_ON(nid < 0 || nid >= MAX_NUMNODES);
    VM_WARN_ON(!node_online(nid));

    return __alloc_pages(gfp_mask, order, node_zonelist(nid, gfp_mask));
}
```

**__alloc_pages**函数直接将自己的所有信息传递给__alloc_pages_nodemask来完成内存的分配

```c
static inline struct page *
__alloc_pages(gfp_t gfp_mask, unsigned int order,
        struct zonelist *zonelist)
{
    return __alloc_pages_nodemask(gfp_mask, order, zonelist, NULL);
}
```

到__alloc_pages_nodemask就进入了比较正式的流程了，主要包含两步：

* 直接分配
* 分配失败选择另一种方式即slowpath继续处理.

首次尝试分配是调用了

```c
static struct page 
*get_page_from_freelist(gfp_t gfp_mask, nodemask_t *nodemask, unsigned int order,struct zonelist *zonelist, int high_zoneidx, int alloc_flags,struct zone *preferred_zone, int migratetype)
```

核心机制就是遍历zonelist上的zone，找到一个page。该函数主要实现功能：

* 在zonelist中找到一个合适的zone 
* 从zone中分配页面。

在选定zone的阶段，在正常情况下需要进行一系列的验证，保证当前zone有足够的可用页面供分配。须携带ALLOC_NO_WATERMARKS标识的，所以这里就分为两种情况。这里涉及到一个分配水位watermark,水位有三种：

```c
enum zone_watermarks {
        WMARK_MIN,
        WMARK_LOW,
        WMARK_HIGH,
        NR_WMARK
};

#define min_wmark_pages(z) (z->watermark[WMARK_MIN])
#define low_wmark_pages(z) (z->watermark[WMARK_LOW])
#define high_wmark_pages(z) (z->watermark[WMARK_HIGH])
```

在分配之前一般会指定满足那个水位才允许分配，或者不管水位直接分配，这就对应ALLOC_NO_WATERMARKS标识。在zone结构中，有vm_stat字段，是一个数组，记录各个状态的页面的数量，其中就包含空闲页面，对应NR_FREE_PAGES，携带watermark标识的分配，需要验证空闲页面是否大于对应的水位，只有在大于水位了才允许分配，否则需要根据情况对页面进行回收reclaim，如果无法回收或者回收后仍然不满足条件，则直接返回了。在一些急迫的事务中，可以指定ALLOC_NO_WATERMARKS，这样会不会对水位进行验证，直接调用buffered_rmqueue分配页面。

**zone_watermark_ok函数检查标志**

设置的标志在zone_watermark_ok函数中检查, 该函数根据设置的标志判断是否能从给定的内存域分配内存。

```c
bool zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
              int classzone_idx, unsigned int alloc_flags)
{
    return __zone_watermark_ok(z, order, mark, classzone_idx, alloc_flags,
                    zone_page_state(z, NR_FREE_PAGES));
}
```

__zone_watermark_ok函数则完成了检查的工作：

```c
/*
 * Return true if free base pages are above 'mark'. For high-order checks it
 * will return true of the order-0 watermark is reached and there is at least
 * one free page of a suitable size. Checking now avoids taking the zone lock
 * to check in the allocation paths if no pages are free.
 */
bool __zone_watermark_ok(struct zone *z, unsigned int order, unsigned long mark,
             int classzone_idx, unsigned int alloc_flags,
             long free_pages)
{
    long min = mark;
    int o;
    const bool alloc_harder = (alloc_flags & ALLOC_HARDER);

    /* free_pages may go negative - that's OK
     * free_pages可能变为负值, 没有关系 */
    free_pages -= (1 << order) - 1;

    if (alloc_flags & ALLOC_HIGH)
        min -= min / 2;

    //将最小值标记降低到当前值的一半或四分之一，使得分配过程努力或更加努力),
    if (likely(!alloc_harder))
        free_pages -= z->nr_reserved_highatomic;
    else
        min -= min / 4;

#ifdef CONFIG_CMA
    /* If allocation can't use CMA areas don't use free CMA pages */
    if (!(alloc_flags & ALLOC_CMA))
        free_pages -= zone_page_state(z, NR_FREE_CMA_PAGES);      //得到空闲页的个数
#endif

   //该函数会检查空闲页的数目free_pages是否小于最小值与lowmem_reserve中指定的紧急分配值min之和.
    if (free_pages <= min + z->lowmem_reserve[classzone_idx])
        return false;

    /* If this is an order-0 request then the watermark is fine */
    if (!order)
        return true;

    /* For a high-order request, check at least one suitable page is free 
     * 在下一阶，当前阶的页是不可用的  */
    for (o = order; o < MAX_ORDER; o++) {
        struct free_area *area = &z->free_area[o];
        int mt;

        if (!area->nr_free)
            continue;

        if (alloc_harder)
            return true;

        /* 所需高阶空闲页的数目相对较少 */
        for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
            if (!list_empty(&area->free_list[mt]))
                return true;
        }

#ifdef CONFIG_CMA
        if ((alloc_flags & ALLOC_CMA) &&
            !list_empty(&area->free_list[MIGRATE_CMA])) {
            return true;
        }
#endif
    }
    return false;
}
```

如果内核遍历所有的低端内存域之后，发现内存不足, 则不进行内存分配。

alloc_flags和gfp_mask之间的区别，gfp_mask是使用alloc_pages申请内存时所传递的申请标记，而alloc_flags是在内存管理子系统内部使用的另一个标记。关于**alloc_flags**的定义有如下几个：

```c
/* The ALLOC_WMARK bits are used as an index to zone->watermark */
#define ALLOC_WMARK_MIN     WMARK_MIN
#define ALLOC_WMARK_LOW     WMARK_LOW
#define ALLOC_WMARK_HIGH    WMARK_HIGH
#define ALLOC_NO_WATERMARKS 0x04 /* don't check watermarks at all */

#define ALLOC_HARDER        0x10 /* try to alloc harder */
#define ALLOC_HIGH      0x20 /* __GFP_HIGH set */
#define ALLOC_CPUSET        0x40 /* check for correct cpuset */                                                                                                                                      
#define ALLOC_CMA       0x80 /* allow allocations from CMA areas */
#define ALLOC_FAIR      0x100 /* fair zone allocation */

```

## get_page_from_freelist

get_page_from_freelist是伙伴系统使用的另一个重要的辅助函数. 它通过标志集和分配阶来判断是否能进行分配。如果可以，则发起实际的分配操作。
get_page_from_freelist将那些相关联的参数封装成一个结构：

```c
static struct page *
get_page_from_freelist(gfp_t gfp_mask, unsigned int order, int alloc_flags, const struct alloc_context *ac)
```

封装好的结构是struct alloc_context：

```c
/*
 * Structure for holding the mostly immutable allocation parameters passed
 * between functions involved in allocations, including the alloc_pages*
 * family of functions.
 *
 * nodemask, migratetype and high_zoneidx are initialized only once in
 * __alloc_pages_nodemask() and then never change.
 *
 * zonelist, preferred_zone and classzone_idx are set first in
 * __alloc_pages_nodemask() for the fast path, and might be later changed
 * in __alloc_pages_slowpath(). All other functions pass the whole strucure
 * by a const pointer.
 */
struct alloc_context {
        struct zonelist *zonelist;
        nodemask_t *nodemask;
        struct zoneref *preferred_zoneref;
        int migratetype;
        enum zone_type high_zoneidx;
        bool spread_dirty_pages;
};
```

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210712221357689.png?x-oss-process=image/watermark,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3dlaXhpbl80OTI5ODkzMQ==,size_16,color_FFFFFF,t_70)



参考链接：

https://kernel.blog.csdn.net/article/details/52704844