#include <defs.h>
#include <x86.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_clock.h>
#include <list.h>

list_entry_t pra_list_head;

static int _clock_init_mm(struct mm_struct *mm) {
    list_init(&pra_list_head);
    mm->sm_priv = &pra_list_head;
    return 0;
}

static int _clock_map_swappable(struct mm_struct *mm, uintptr_t addr,
								struct Page *page, int swap_in) {
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    list_entry_t *entry = &(page->pra_page_link);
    assert(entry != NULL && head != NULL);
    //将entry加入到list的头
    list_add(head, entry);
    SetDirtyBit(page);
    //init present_ptr
    if(mm->present_ptr == NULL){
        cprintf("INIT\n\n\n\n");
        mm->present_ptr = entry;
    }
    return 0;
}

static int _clock_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page,
								  int in_tick) {
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    list_entry_t *tmp;
    struct Page *iter;
    assert(head != NULL);
    assert(in_tick == 0);
    //cprintf("Present PTR %x\n", mm->present_ptr);
    cprintf("\nLet's find victim!!!\n");
    //list empty
    assert(head -> next != head);
    while(1){
        //若转了一圈回到了head 跳过head
        if(mm->present_ptr == head) {
            cprintf("Hit head!\n");
            mm->present_ptr = mm->present_ptr->next;
        }
        iter = le2page(mm->present_ptr,pra_page_link);
        //若标记dirtybit为0就替换走
        if(!PageDirty(iter)) {
            cprintf("Find it!!!\n\n");
            tmp = mm->present_ptr;
            mm->present_ptr = mm->present_ptr->next;
            *ptr_page = iter;
            list_del(tmp);
            return 0;
        }
        cprintf("Not good\n");
        ClearDirtyBit(iter);
        mm->present_ptr = mm->present_ptr->next;
    }
    return 1;
}

static void visit(struct mm_struct *mm,uintptr_t addr,int value){
    *(unsigned char*) addr = value;
    //从虚拟地址获取pte
    uintptr_t *tmp = get_pte(mm->pgdir,addr,0);
    //从pte获取page
    struct Page *page = pte2page(*tmp);
    //设置dirty_bit
    SetDirtyBit(page);
}

/*
 * _clock_init
 */
static int _clock_init(void) {
    return 0;
}

/*
 * _clock_set_unswappable
 * 设置addr为不可被换出的
 */
static int _clock_set_unswappable(struct mm_struct *mm, uintptr_t addr) {
    list_entry_t *head = (list_entry_t*) mm->sm_priv;
    list_entry_t *iter = head->next;
    //从虚拟地址获取pte
    uintptr_t *tmp = get_pte(mm->pgdir,addr,0);
    //从pte获取page
    struct Page *page = pte2page(*tmp);
    while(iter != head){
        if(iter == &(page->pra_page_link)){
            list_del(iter);
            return 0;
        }
        iter = iter->next;
    }
    cprintf("Cannot find addr in list");
    assert(0);
    return 1;
}

static int _clock_tick_event(struct mm_struct *mm) {
    return 0;
}

static int _clock_check_swap(struct mm_struct *mm) {
    cprintf("write Virt Page c in clock_check_swap\n");
    visit(mm,0x3000,0x0c);
    cprintf("PGFAULT_NUM:%d\n",pgfault_num);

    cprintf("write Virt Page a in clock_check_swap\n");
    visit(mm,0x1000,0x0a);
    //assert(pgfault_num == 4);
    cprintf("PGFAULT_NUM:%d\n",pgfault_num);

    cprintf("write Virt Page d in clock_check_swap\n");
    visit(mm,0x4000, 0x0d);
    //assert(pgfault_num == 4);
    cprintf("PGFAULT_NUM:%d\n",pgfault_num);

    cprintf("write Virt Page b in clock_check_swap\n");
    visit(mm,0x2000 ,0x0b);
    //assert(pgfault_num == 4);
    cprintf("PGFAULT_NUM:%d\n",pgfault_num);

    cprintf("write Virt Page e in clock_check_swap\n");
    visit(mm,0x5000 , 0x0e);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);
    //assert(pgfault_num == 5);

    cprintf("write Virt Page b in clock_check_swap\n");
    visit(mm,0x2000, 0x0b);
    //assert(pgfault_num == 5);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);

    cprintf("write Virt Page a in clock_check_swap\n");
    visit(mm, 0x1000 , 0x0a);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);
    //assert(pgfault_num == 6);

    cprintf("write Virt Page b in clock_check_swap\n");
    visit(mm, 0x2000 , 0x0b);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);
    //assert(pgfault_num == 7);

    cprintf("write Virt Page c in clock_check_swap\n");
    visit(mm, 0x3000 , 0x0c);
    //assert(pgfault_num == 8);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);

    cprintf("write Virt Page d in clock_check_swap\n");
    visit(mm, 0x4000 , 0x0d);
    cprintf("PGFAULT_NUM:%d\n\n",pgfault_num);
    //assert(pgfault_num == 9);
    return 0;
}

struct swap_manager swap_manager_clock = {
	.name = "clock swap manager",
	.init = &_clock_init,
	.init_mm = &_clock_init_mm,
	.tick_event = &_clock_tick_event,
	.map_swappable = &_clock_map_swappable,
	.set_unswappable = &_clock_set_unswappable,
	.swap_out_victim = &_clock_swap_out_victim,
	.check_clock_swap = &_clock_check_swap,
};
