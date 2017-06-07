#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/* In the first fit algorithm, the allocator keeps a list of free blocks (known as the free list) and,
   on receiving a request for memory, scans along the list for the first block that is large enough to
   satisfy the request. If the chosen block is significantly larger than that requested, then it is 
   usually split, and the remainder added to the list as another free block.
   Please see Page 196~198, Section 8.2 of Yan Wei Min's chinese book "Data Structure -- C programming language"
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: default_init,default_init_memmap,default_alloc_pages, default_free_pages.
/*
 * Details of FFMA
 * (1) Prepare: In order to implement the First-Fit Mem Alloc (FFMA), we should manage the free mem block use some list.
 *              The struct free_area_t is used for the management of free mem blocks. At first you should
 *              be familiar to the struct list in list.h. struct list is a simple doubly linked list implementation.
 *              You should know howto USE: list_init, list_add(list_add_after), list_add_before, list_del, list_next, list_prev
 *              Another tricky method is to transform a general list struct to a special struct (such as struct page):
 *              you can find some MACRO: le2page (in memlayout.h), (in future labs: le2vma (in vmm.h), le2proc (in proc.h),etc.)
 * (2) default_init: you can reuse the  demo default_init fun to init the free_list and set nr_free to 0.
 *              free_list is used to record the free mem blocks. nr_free is the total number for free mem blocks.
 * (3) default_init_memmap:  CALL GRAPH: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 *              This fun is used to init a free block (with parameter: addr_base, page_number).
 *              First you should init each page (in memlayout.h) in this free block, include:
 *                  p->flags should be set bit PG_property (means this page is valid. In pmm_init fun (in pmm.c),
 *                  the bit PG_reserved is setted in p->flags)
 *                  if this page  is free and is not the first page of free block, p->property should be set to 0.
 *                  if this page  is free and is the first page of free block, p->property should be set to total num of block.
 *                  p->ref should be 0, because now p is free and no reference.
 *                  We can use p->page_link to link this page to free_list, (such as: list_add_before(&free_list, &(p->page_link)); )
 *              Finally, we should sum the number of free mem block: nr_free+=n
 * (4) default_alloc_pages: search find a first free block (block size >=n) in free list and reszie the free block, return the addr
 *              of malloced block.
 *              (4.1) So you should search freelist like this:
 *                       list_entry_t le = &free_list;
 *                       while((le=list_next(le)) != &free_list) {
 *                       ....
 *                 (4.1.1) In while loop, get the struct page and check the p->property (record the num of free block) >=n?
 *                       struct Page *p = le2page(le, page_link);
 *                       if(p->property >= n){ ...
 *                 (4.1.2) If we find this p, then it' means we find a free block(block size >=n), and the first n pages can be malloced.
 *                     Some flag bits of this page should be setted: PG_reserved =1, PG_property =0
 *                     unlink the pages from free_list
 *                     (4.1.2.1) If (p->property >n), we should re-caluclate number of the the rest of this free block,
 *                           (such as: le2page(le,page_link))->property = p->property - n;)
 *                 (4.1.3)  re-caluclate nr_free (number of the the rest of all free block)
 *                 (4.1.4)  return p
 *               (4.2) If we can not find a free block (block size >=n), then return NULL
 * (5) default_free_pages: relink the pages into  free list, maybe merge small free blocks into big free blocks.
 *               (5.1) according the base addr of withdrawed blocks, search free list, find the correct position
 *                     (from low to high addr), and insert the pages. (may use list_next, le2page, list_add_before)
 *               (5.2) reset the fields of pages, such as p->ref, p->flags (PageProperty)
 *               (5.3) try to merge low addr or high addr blocks. Notice: should change some pages's p->property correctly.
 */
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free)
/**
 * default_init:
 * you can reuse the  demo default_init fun to
 * init the free_list and set nr_free to 0.
 * 代码已经实现了 init freelist 然后设置nr_free 为0
 * free_list : is used to record the free mem blocks.
 * nr_free   : is the total number for free mem blocks.
 */
static void default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}

/**
 * default_init_memmap:
 * 调用顺序: kern_init --> pmm_init-->page_init-->init_memmap--> pmm_manager->init_memmap
 * This fun is used to init a free block (with parameter: addr_base, page_number).
 * 这个函数是用来初始化一个free block 参数有base和page_number
 * 若本页是空的 且不是第一页，flag中的property位设置为0
 * 若本页是空的 且是第一页，flag中的property位设置为1 说明其有效
 * 若本页空 且是第一页 property就是总共的空block数
 * reference 引用的count清0 也就是有没有对应咯
 **/
static void default_init_memmap(struct Page *base, size_t n) {
    //assert用来检查一个判断是否为真
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p++) {
        p->flags = 0;
        //若本页空&&不是第一页,property就是总共的空block数
        p->property = 0;
        //reference引用的count清0
        p->ref = 0;
        ClearPageReserved(p);
        // from memlayout
        // if this bit=1:
        // the Page is reserved for kernel,
        // cannot be used in alloc/free_pages;
        // otherwise, this bit=0
    }
    //跟新一共有多少个free page
    nr_free += n;
    //first block
    SetPageProperty(base);
    //若本页空&&第一页,property就是总共的空block数
    base->property = n;
    list_add(&free_list, &(base->page_link));
}

/**
 * default_alloc_pages:
 * search find a first free block (block size >=n)
 * in free list and reszie the free block,
 * return the addr of malloced block.
 **/
static struct Page *
default_alloc_pages(size_t n) {
    list_entry_t *list_iterator, *nxtptr;
    struct Page *first_fit_page;
    assert(n > 0);
    //要分配的页数大于剩余的free数
    if (n > nr_free) {
        return NULL;
    }
    list_iterator = &free_list;
    //循环访问一次free list
    while ((list_iterator = list_next(list_iterator)) != &free_list) {
        //从link回去找到page的base addr
        first_fit_page = le2page(list_iterator, page_link);
        //若当前的page可用大于需要的size
        //注意这个地方，只有一个空闲区域里面的first page才会有property的data
        if (first_fit_page->property >= n) {
            //将这片空闲区域中的n个block分配出去
            //检查这n个block能否分配
            struct Page *alloc = first_fit_page;
            for (; alloc != first_fit_page + n; alloc++) {
                //若Reserved位为1 不能alloc和free
                assert(!PageReserved(alloc));
            }
            //若这个空闲区域里面空闲的数量比我需要的数量还要多
            if (first_fit_page->property > n) {
                //原来空闲的空间这样子 H是空闲区域里面first page
                //||H1....................x..................||
                //现在这个空闲的区域变成了这样子
                //.......n.......||H2........x-n.............||
                //设置其property为剩下的x-n个block
                //向后找n个page后的page
                struct Page *new_head = first_fit_page + n;
                new_head->property = first_fit_page->property - n;
                SetPageProperty(new_head);
                //因为按照addr的order 则直接添加到first fit page的后面就好了
                list_add(&(first_fit_page->page_link), &(new_head->page_link));
            }
            //从free list 中删除分配走的page的link
            list_del(&(first_fit_page->page_link));
            ClearPageProperty(first_fit_page);
            nr_free -= n;
            return first_fit_page;
        }
    }
    return NULL;
}

/**
 * default_free_pages
 * free掉分配了的页
 * base：为free的页的first page的基地址
 * n   ：为free掉多少页
 */
static void default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    //将n这么多空间释放
    for (; p != base + n; p++) {
        //检查要free的n个size时候合法
        //不能被标记Reserved和Property
        assert(!PageReserved(p) && !PageProperty(p));
        //初始化flags位
        p->flags = 0;
        //清空页面访问counter
        set_page_ref(p, 0);
    }
    //更新新的base
    base->property = n;
    SetPageProperty(base);

    //下面开始找能否合并到其他的已有的区域
    list_entry_t *le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        le = list_next(le);
        //若base后面接着了p
        if (base + base->property == p) {
            base->property += p->property;
            p->property = 0;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
        //若p后面接着了base
        else if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            base->property = 0;
            base = p;
            list_del(&(p->page_link));
        }
    }
    // 按照地址顺序插入这个新的base节点
    nr_free += n;
    le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        if (p > base)
            break;
        le = list_next(le);
    }
    //插入到list中去
    list_add_before(le, &(base->page_link));
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

