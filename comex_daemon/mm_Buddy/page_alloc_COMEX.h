#include <stdio.h>
#include <stdlib.h>
#include "list.h"

#define set_page_private(page, v)       ((page)->private = (v))
#define page_private(page)              ((page)->private)
#define pfn_valid_within(pfn) 	(1)
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

///////////////// Deallocation///////////////////

unsigned long page_to_pfn(struct page *page){
	return page->pageNO;
}
static inline unsigned long page_order(struct page *page)
{
	 /* PageBuddy() must be checked by the caller */
	 return page_private(page);
}
static inline int PageBuddy(struct page *page)
{
	return (page->_mapcount == PAGE_BUDDY_MAPCOUNT_VALUE);
}
static inline int page_is_buddy(struct page *page, struct page *buddy, int order)
{
	if (PageBuddy(buddy) && page_order(buddy) == order) {
		return 1;
	}
	return 0;
}
static inline unsigned long
__find_combined_index(unsigned long page_idx, unsigned int order)
{
	return (page_idx & ~(1 << order));
}

/*
 * Locate the struct page for both the matching buddy in our
 * pair (buddy1) and the combined O(n+1) page they form (page).
 *
 * 1) Any buddy B1 will have an order O twin B2 which satisfies
 * the following equation:
 *     B2 = B1 ^ (1 << O)
 * For example, if the starting buddy (buddy2) is #8 its order
 * 1 buddy is #10:
 *     B2 = 8 ^ (1 << 1) = 8 ^ 2 = 10
 *
 * 2) Any buddy B will have an order O+1 parent P which
 * satisfies the following equation:
 *     P = B & ~(1 << O)
 *
 * Assumption: *_mem_map is contiguous at least up to MAX_ORDER
 */
static inline struct page *
__page_find_buddy(struct page *page, unsigned long page_idx, unsigned int order)
{
	unsigned long buddy_idx = page_idx ^ (1 << order);

	return page + (buddy_idx - page_idx);
}

/*
 * Freeing function for a buddy system allocator.
 *
 * The concept of a buddy system is to maintain direct-mapped table
 * (containing bit values) for memory blocks of various "orders".
 * The bottom level table contains the map for the smallest allocatable
 * units of memory (here, pages), and each level above it describes
 * pairs of units from the levels below, hence, "buddies".
 * At a high level, all that happens here is marking the table entry
 * at the bottom level available, and propagating the changes upward
 * as necessary, plus some accounting needed to play nicely with other
 * parts of the VM system.
 * At each level, we keep a list of pages, which are heads of continuous
 * free pages of length of (1 << order) and marked with PG_buddy. Page's
 * order is recorded in page_private(page) field.
 * So when we are allocating or freeing one, we can derive the state of the
 * other.  That is, if we allocate a small block, and both were   
 * free, the remainder of the region must be split into blocks.   
 * If a block is freed, and its buddy is also free, then this
 * triggers coalescing into a block of larger size.            
 *
 * -- wli
 */

static inline void __free_one_page(int pageNum, unsigned int order)
{
	unsigned long page_idx;
	unsigned long combined_idx;
	int migratetype = 0;
	struct zone *zone = COMEXzone;
	struct page *buddy;
	struct page *page = &(COMEXpages[pageNum]);

//	if (unlikely(PageCompound(page)))
//		if (unlikely(destroy_compound_page(page, order)))
//			return;

	page_idx = page_to_pfn(page) & ((1 << MAX_ORDER) - 1);
	printf("page_idx %lu\n", page_idx);

	while (order < MAX_ORDER-1) {
		buddy = __page_find_buddy(page, page_idx, order);
//		printf("Buddy %lu\n", buddy->pageNO );
		if (!page_is_buddy(page, buddy, order)){
			printf("!page_is_buddy \n");
			break;
		}

	//	 Our buddy is free, merge with it and move up one order. 
		list_del(&buddy->lru);
		zone->free_area[order].nr_free--;
		rmv_page_order(buddy);
		combined_idx = __find_combined_index(page_idx, order);
		page = page + (combined_idx - page_idx);
		page_idx = combined_idx;
		order++;
	}
	set_page_order(page, order);

	///
	 // If this is not the largest possible page, check if the buddy
	 // of the next-highest order is free. If it is, it's possible
	 // that pages are being freed that will coalesce soon. In case,
	 // that is happening, add the free page to the tail of the list
	 // so it's less likely to be used soon and more likely to be merged
	 // as a higher order page
	 ///
	if ((order < MAX_ORDER-2) && pfn_valid_within(page_to_pfn(buddy))) {
		struct page *higher_page, *higher_buddy;
		combined_idx = __find_combined_index(page_idx, order);
		higher_page = page + combined_idx - page_idx;
		higher_buddy = __page_find_buddy(higher_page, combined_idx, order + 1);
		if (page_is_buddy(higher_page, higher_buddy, order + 1)) {
			list_add_tail(&page->lru,
				&zone->free_area[order].free_list[migratetype]);
			goto out;
		}
	}

	list_add(&page->lru, &zone->free_area[order].free_list[migratetype]);
out:
	zone->free_area[order].nr_free++;
	
}

////////////////////////////////////

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
