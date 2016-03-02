static inline void COMEX_ClearPageBuddy(COMEX_page *page){
	atomic_set(&page->_mapcount, -1);
}

static inline void COMEX_rmv_page_order(COMEX_page *page){
	COMEX_ClearPageBuddy(page);
	set_page_private(page, 0);
}

static inline void COMEX_SetPageBuddy(COMEX_page *page){
	atomic_set(&page->_mapcount, (-128));
}

static inline void COMEX_set_page_order(COMEX_page *page, int order){
	set_page_private(page, order);
	COMEX_SetPageBuddy(page);
}

static inline void COMEX_expand(COMEX_Zone *zone, COMEX_page *page,	
	int low, int high, struct COMEX_free_area *area,int migratetype){
	
	unsigned long size = 1 << high;
	while (high > low) {
		area--;
		high--;
		size >>= 1;
//		VM_BUG_ON(bad_range(zone, &page[size]));
		list_add(&page[size].lru, &area->free_list[migratetype]);
		area->nr_free++;
		COMEX_set_page_order(&page[size], high);
	}
}

unsigned long COMEX_page_to_pfn(COMEX_page *page){
	return page->pageNO;
}

static inline int COMEX_PageBuddy(COMEX_page *page){
	return atomic_read(&page->_mapcount) == (-128);
}

static inline unsigned long COMEX_page_order(COMEX_page *page){
	return page->private;
}

static inline int COMEX_page_is_buddy(COMEX_page *page, COMEX_page *buddy, int order){
	if (COMEX_PageBuddy(buddy) && COMEX_page_order(buddy) == order){
		return 1;
	}
	return 0;
}

static inline COMEX_page *
COMEX_page_find_buddy(COMEX_page *page, unsigned long page_idx, unsigned int order){
	unsigned long buddy_idx = page_idx ^ (1 << order);

	return page + (buddy_idx - page_idx);
}

static inline unsigned long
COMEX_find_combined_index(unsigned long page_idx, unsigned int order){
	return (page_idx & ~(1 << order));
}

static inline void COMEX_free_one_page(unsigned long inPageNO, unsigned int order){
	unsigned long page_idx;
	unsigned long combined_idx;
	int migratetype = 0;
	COMEX_page *buddy;
	COMEX_page *page = &COMEX_Buddy_page[inPageNO];
	COMEX_Zone *zone = COMEX_Buddy_Zone;

//	printk(KERN_INFO "%s: PageNO %lu\n", __FUNCTION__, page->pageNO);
	page_idx = COMEX_page_to_pfn(page) & ((1 << COMEX_MAX_ORDER) - 1);

	while (order < COMEX_MAX_ORDER-1) {
		buddy = COMEX_page_find_buddy(page, page_idx, order);
		if (!COMEX_page_is_buddy(page, buddy, order))
			break;

		/* Our buddy is free, merge with it and move up one order. */
		list_del(&buddy->lru);
		zone->free_area[order].nr_free--;
		COMEX_rmv_page_order(buddy);
		combined_idx = COMEX_find_combined_index(page_idx, order);
		page = page + (combined_idx - page_idx);
		page_idx = combined_idx;
		order++;
	}
	COMEX_set_page_order(page, order);

	/*
	 * If this is not the largest possible page, check if the buddy
	 * of the next-highest order is free. If it is, it's possible
	 * that pages are being freed that will coalesce soon. In case,
	 * that is happening, add the free page to the tail of the list
	 * so it's less likely to be used soon and more likely to be merged
	 * as a higher order page
	 */
	if ((order < COMEX_MAX_ORDER-2) && pfn_valid_within(page_to_pfn(buddy))) {
		COMEX_page *higher_page, *higher_buddy;
		combined_idx = COMEX_find_combined_index(page_idx, order);
		higher_page = page + combined_idx - page_idx;
		higher_buddy = COMEX_page_find_buddy(higher_page, combined_idx, order + 1);
		if (COMEX_page_is_buddy(higher_page, higher_buddy, order + 1)) {
			list_add_tail(&page->lru,
				&zone->free_area[order].free_list[migratetype]);
			goto out;
		}
	}

	list_add(&page->lru, &zone->free_area[order].free_list[migratetype]);
out:
	zone->free_area[order].nr_free++;
	
}

/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static inline
unsigned long COMEX_rmqueue_smallest(unsigned int order){
	COMEX_Zone *zone = COMEX_Buddy_Zone;
	int migratetype = 0;
	
	unsigned int current_order;
	struct COMEX_free_area * area;
	COMEX_page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < COMEX_MAX_ORDER; ++current_order) {
		area = &(zone->free_area[current_order]);
		if (list_empty(&area->free_list[migratetype]))
			continue;

		page = list_entry(area->free_list[migratetype].next, COMEX_page, lru);
		list_del(&page->lru);
		COMEX_rmv_page_order(page);
		area->nr_free--;
		COMEX_expand(zone, page, order, current_order, area, migratetype);
		return page->pageNO + 1;
	}

	return 0;
}

unsigned long COMEX_get_from_Buddy(int order){
	unsigned long pageNO = 0;
	
	spin_unlock_wait(&COMEX_Buddy_lock);
	spin_lock(&COMEX_Buddy_lock);
	pageNO = COMEX_rmqueue_smallest(order);
	spin_unlock(&COMEX_Buddy_lock);
	
//	printk(KERN_INFO "%s: Allocate %lu\n", __FUNCTION__, pageNO);	
	if(pageNO > 0)
		return (pageNO-1)*X86PageSize;
	
	return 200;
}

void COMEX_free_to_Buddy(unsigned long pageNO, unsigned int order){
	
	spin_unlock_wait(&COMEX_Buddy_lock);
	spin_lock(&COMEX_Buddy_lock);
	COMEX_free_one_page(pageNO, order);
	spin_unlock(&COMEX_Buddy_lock);
	
//	printk(KERN_INFO "%s: Free %lu\n", __FUNCTION__, pageNO);	
}

void COMEX_free_to_Buddy_Addr(unsigned long Addr, unsigned int order){

	unsigned long pageNO;
	pageNO = (Addr - COMEX_start_addr)/X86PageSize;

	spin_unlock_wait(&COMEX_Buddy_lock);
	spin_lock(&COMEX_Buddy_lock);
	COMEX_free_one_page(pageNO, order);
	spin_unlock(&COMEX_Buddy_lock);
	
//	printk(KERN_INFO "%s: Free %lu\n", __FUNCTION__, pageNO);	
}

void print_free_blocks(void){
	int i;
	
	for(i=0; i<COMEX_MAX_ORDER; i++){
		printk(KERN_INFO "%s: Order %d - %lu\n", __FUNCTION__, i, COMEX_Buddy_Zone->free_area[i].nr_free);
	}
	printk(KERN_INFO "%s: \n", __FUNCTION__);
}