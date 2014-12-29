#define MAX_ORDER 11
#define MIGRATE_TYPES 1

#define atomic_t unsigned long

struct list_head {
	struct list_head *next, *prev;
};

struct free_area {
	struct list_head	free_list[MIGRATE_TYPES];
	unsigned long		nr_free;
};

struct zone {
	struct free_area	free_area[MAX_ORDER];
};

typedef struct page {
	unsigned long pageNO;
	atomic_t _count;
	atomic_t _mapcount;
	unsigned long private;
	struct list_head lru;
} pageStruct;
