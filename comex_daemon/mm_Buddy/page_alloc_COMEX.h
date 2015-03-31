#include <stdio.h>
#include <stdlib.h>
#include "list.h"

#define set_page_private(page, v)       ((page)->private = (v))
#define BIGGEST_GROUP 1024
#define PAGE_BUDDY_MAPCOUNT_VALUE (-128)

struct page *COMEXpages;
struct zone *COMEXzone;

static inline void __ClearPageBuddy(struct page *page)
{
   // VM_BUG_ON_PAGE(!PageBuddy(page), page);
    page->_mapcount = -1;
}

static inline void rmv_page_order(pageStruct *page)
{
	__ClearPageBuddy(page);
	set_page_private(page, 0);
}

static inline void __SetPageBuddy(struct page *page)
{
    //VM_BUG_ON_PAGE(atomic_read(&page->_mapcount) != -1, page);
	page->_mapcount = PAGE_BUDDY_MAPCOUNT_VALUE;
}

static inline void set_page_order(struct page *page, int order)
{
	set_page_private(page, order);
	__SetPageBuddy(page);
}

/*
 * The order of subdivision here is critical for the IO subsystem.
 * Please do not alter this order without good reasons and regression
 * testing. Specifically, as large blocks of memory are subdivided,
 * the order in which smaller blocks are delivered depends on the order
 * they're subdivided in this function. This is the primary factor
 * influencing the order in which pages are delivered to the IO
 * subsystem according to empirical testing, and this is also justified
 * by considering the behavior of a buddy system containing a single
 * large block of memory acted on by a series of small allocations.
 * This behavior is a critical factor in sglist merging's success.
 *
 * -- wli
 */
static inline void expand(struct zone *zone, struct page *page,
	int low, int high, struct free_area *area,
	int migratetype)
{
	unsigned long size = 1 << high;

	while (high > low) {
		area--;
		high--;
		size >>= 1;
		//VM_BUG_ON(bad_range(zone, &page[size]));
		list_add(&page[size].lru, &area->free_list[migratetype]);
		area->nr_free++;
		set_page_order(&page[size], high);
	}
}

/*
 * Go through the free lists for the given migratetype and remove
 * the smallest available page from the freelists
 */
static inline
struct page *__rmqueue_smallest(unsigned int order)
{
	unsigned int current_order;
	struct free_area * area;
	struct page *page;

	/* Find a page of the appropriate size in the preferred list */
	for (current_order = order; current_order < MAX_ORDER; ++current_order) {
		area = &(COMEXzone->free_area[current_order]);
		if (list_empty(&area->free_list[0]))
			continue;

		page = list_entry(area->free_list[0].next, struct page, lru);
		list_del(&page->lru);
		rmv_page_order(page);
		area->nr_free--;
		expand(COMEXzone, page, order, current_order, area, 0);
		return page;
	}

	return NULL;
}

void print_free_list(){
	int i,j;
	struct page *PrintPage;
	
	for(i=0; i<MAX_ORDER; i++){
		printf("free_area %2d: ",i);
		if(COMEXzone->free_area[i].nr_free)
			PrintPage = list_entry(COMEXzone->free_area[i].free_list[0].next, struct page, lru);
			
		for(j=0; j<COMEXzone->free_area[i].nr_free; j++){
			printf("%ld ", PrintPage->pageNO);
			PrintPage = list_entry(PrintPage->lru.next, struct page, lru);
		}
		printf("\n");
	}
	printf("\n");	
}

int initBuddy(long N_Pages){
	int i;
	
	COMEXpages = (struct page*)malloc(sizeof(struct page)*N_Pages);		// Create COMEX's page descriptor
	COMEXzone = (struct zone*)malloc(sizeof(struct zone));		// Zone descriptor (only 1)
	for(i=0; i<MAX_ORDER; i++){
		INIT_LIST_HEAD(&COMEXzone->free_area[i].free_list[0]);	// Init list head for each freelist
	}	
	
	for(i=0; i<N_Pages; i++){
		COMEXpages[i].pageNO = i;
		COMEXpages[i].private = MAX_ORDER-1;		
		INIT_LIST_HEAD(&(COMEXpages[i].lru));
		
		if(i%BIGGEST_GROUP == 0){
			list_add_tail(&(COMEXpages[i].lru), &COMEXzone->free_area[MAX_ORDER-1].free_list[0]);
			COMEXzone->free_area[MAX_ORDER-1].nr_free++;
		}
	}
	return 1;
}
