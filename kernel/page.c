#include "os.h"

/*
 * Following global vars are defined in mem.S
 */
extern uint32_t TEXT_START;
extern uint32_t TEXT_END;
extern uint32_t DATA_START;
extern uint32_t DATA_END;
extern uint32_t RODATA_START;
extern uint32_t RODATA_END;
extern uint32_t BSS_START;
extern uint32_t BSS_END;
extern uint32_t HEAP_START;
extern uint32_t HEAP_SIZE;

/*
 * _alloc_start points to the actual start address of heap pool
 * _alloc_end points to the actual end address of heap pool
 * _num_pages holds the actual max number of pages we can allocate.
 */
static uint32_t _alloc_start = 0;
static uint32_t _alloc_end = 0;
static uint32_t _num_pages = 0;

#define PAGE_SIZE 256
#define PAGE_ORDER 8

#define PAGE_TAKEN (uint8_t)(1 << 0)
#define PAGE_LAST  (uint8_t)(1 << 1)

/*
 * Page Descriptor 
 * flags:
 * - bit 0: flag if this page is taken(allocated)
 * - bit 1: flag if this page is the last page of the memory block allocated
 */
struct Page {
	uint8_t flags;
};

static inline void _clear(struct Page *page)
{
	page->flags = 0;
}

static inline int _is_free(struct Page *page)
{
	if (page->flags & PAGE_TAKEN) {
		return 0;
	} else {
		return 1;
	}
}

static inline void _set_flag(struct Page *page, uint8_t flags)
{
	page->flags |= flags;
}

static inline int _is_last(struct Page *page)
{
	if (page->flags & PAGE_LAST) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * align the address to the border of page(4K)
 */
static inline uint32_t _align_page(uint32_t address)
{
	uint32_t order = (1 << PAGE_ORDER) - 1;
	return (address + order) & (~order);
}

static int heap_extend(uint32_t need);

void page_init()
{
	/* 
	 * We reserved 8 Page (8 x 4096) to hold the Page structures.
	 * It should be enough to manage at most 128 MB (8 x 4096 x 4096) 
	 */
	_num_pages = (HEAP_SIZE / PAGE_SIZE) - 2048;
	kprintf("HEAP_START = %x, HEAP_SIZE = %x, num of pages = %d\n", HEAP_START, HEAP_SIZE, _num_pages);
	
	struct Page *page = (struct Page *)HEAP_START;
	for (int i = 0; i < _num_pages; i++) {
		_clear(page);
		page++;	
	}

	_alloc_start = _align_page(HEAP_START + 2048 * PAGE_SIZE);
	_alloc_end = _alloc_start + (PAGE_SIZE * _num_pages);

	kprintf("TEXT:   0x%x -> 0x%x\n", TEXT_START, TEXT_END);
	kprintf("RODATA: 0x%x -> 0x%x\n", RODATA_START, RODATA_END);
	kprintf("DATA:   0x%x -> 0x%x\n", DATA_START, DATA_END);
	kprintf("BSS:    0x%x -> 0x%x\n", BSS_START, BSS_END);
	kprintf("HEAP:   0x%x -> 0x%x\n", _alloc_start, _alloc_end);

	heap_extend(PAGE_SIZE);
}

/*
 * Allocate a memory block which is composed of contiguous physical pages
 * - npages: the number of PAGE_SIZE pages to allocate
 */
static void *page_alloc(int npages)
{
	/* Note we are searching the page descriptor bitmaps. */
	int found = 0;
	struct Page *page_i = (struct Page *)HEAP_START;
	for (int i = 0; i <= (_num_pages - npages); i++) {
		if (_is_free(page_i)) {
			found = 1;
			/* 
			 * meet a free page, continue to check if following
			 * (npages - 1) pages are also unallocated.
			 */
			struct Page *page_j = page_i;
			for (int j = i; j < (i + npages); j++) {
				if (!_is_free(page_j)) {
					found = 0;
					break;
				}
				page_j++;
			}
			/*
			 * get a memory block which is good enough for us,
			 * take housekeeping, then return the actual start
			 * address of the first page of this memory block
			 */
			if (found) {
				struct Page *page_k = page_i;
				for (int k = i; k < (i + npages); k++) {
					_set_flag(page_k, PAGE_TAKEN);
					page_k++;
				}
				page_k--;
				_set_flag(page_k, PAGE_LAST);
				return (void *)(_alloc_start + i * PAGE_SIZE);
			}
		}
		page_i++;
	}
	return NULL;
}

/*
 * Free the memory block
 * - p: start address of the memory block
 */
 static void page_free(void *p)
{
	/*
	 * Assert (TBD) if p is invalid
	 */
	if (!p || (uint32_t)p >= _alloc_end) {
		return;
	}
	/* get the first page descriptor of this memory block */
	struct Page *page = (struct Page *)HEAP_START;
	page += ((uint32_t)p - _alloc_start)/ PAGE_SIZE;
	/* loop and clear all the page descriptors of the memory block */
	while (!_is_free(page)) {
		if (_is_last(page)) {
			_clear(page);
			break;
		} else {
			_clear(page);
			page++;;
		}
	}
}

#define ALIGNMENT 8u
#define ALIGN_UP(x, a) (((x) + ((a) - 1)) & ~((a) - 1))

typedef struct Block {
    // size including header+footer, low bit is alloc flag
    uint32_t size_flags;
    struct Block *prev_free;
    struct Block *next_free;
} Block;

typedef uint32_t BlockFooter;

#define BLK_ALLOC  (1u)
#define BLK_SIZE_MASK (~0x7u)

static inline uint32_t blk_size(Block *b) { return b->size_flags & BLK_SIZE_MASK; }
static inline int blk_is_alloc(Block *b) { return (b->size_flags & BLK_ALLOC) != 0; }

static inline void blk_set(Block *b, uint32_t size, int alloc)
{
    b->size_flags = (size & BLK_SIZE_MASK) | (alloc ? BLK_ALLOC : 0u);
    BlockFooter *f = (BlockFooter *)((uint8_t *)b + (size & BLK_SIZE_MASK) - sizeof(BlockFooter));
    *f = b->size_flags;
}

static inline Block *blk_next(Block *b) { return (Block *)((uint8_t *)b + blk_size(b)); }
static inline BlockFooter *blk_prev_footer(Block *b) { return (BlockFooter *)((uint8_t *)b - sizeof(BlockFooter)); }

static inline Block *blk_prev(Block *b)
{
    uint32_t prev_sf = *blk_prev_footer(b);
    uint32_t prev_sz = prev_sf & BLK_SIZE_MASK;
    return (Block *)((uint8_t *)b - prev_sz);
}

static const uint32_t MIN_BLOCK_SIZE =
    (uint32_t)ALIGN_UP(sizeof(Block) + sizeof(BlockFooter), ALIGNMENT);

static Block *_free_head = NULL;

static void freelist_remove(Block *b)
{
    if (!b) return;
    if (b->prev_free) b->prev_free->next_free = b->next_free;
    if (b->next_free) b->next_free->prev_free = b->prev_free;
    if (_free_head == b) _free_head = b->next_free;
    b->prev_free = b->next_free = NULL;
}

static void freelist_insert(Block *b)
{
    if (!b) return;
    b->prev_free = NULL;
    b->next_free = _free_head;
    if (_free_head) _free_head->prev_free = b;
    _free_head = b;
}

// Extend heap by acquiring at least "need" bytes.
// Layout: [prologue alloc][big free][epilogue alloc]
static int heap_extend(uint32_t need)
{
    uint32_t want = need + 2u * MIN_BLOCK_SIZE;
    uint32_t bytes = (uint32_t)ALIGN_UP(want, PAGE_SIZE);
    int npages = (int)(bytes / PAGE_SIZE);
    if (npages <= 0) npages = 1;

    void *seg = page_alloc(npages);
    if (!seg) return 0;

    uint8_t *base = (uint8_t *)seg;
    uint32_t seg_bytes = (uint32_t)npages * PAGE_SIZE;

    Block *pro = (Block *)base;
    blk_set(pro, MIN_BLOCK_SIZE, 1);

    Block *epi = (Block *)(base + seg_bytes - MIN_BLOCK_SIZE);
    blk_set(epi, MIN_BLOCK_SIZE, 1);

    Block *mid = (Block *)(base + MIN_BLOCK_SIZE);
    uint32_t mid_size = seg_bytes - 2u * MIN_BLOCK_SIZE;
    blk_set(mid, mid_size, 0);
    freelist_insert(mid);
    return 1;
}

static Block *find_fit(uint32_t total)
{
    Block *cur = _free_head;
    while (cur) {
        if (blk_size(cur) >= total) return cur;
        cur = cur->next_free;
    }
    return NULL;
}

static void split_and_alloc(Block *b, uint32_t total)
{
    uint32_t bsz = blk_size(b);
    freelist_remove(b);

    if (bsz >= total + MIN_BLOCK_SIZE) {
        blk_set(b, total, 1);
        Block *rem = (Block *)((uint8_t *)b + total);
        blk_set(rem, bsz - total, 0);
        freelist_insert(rem);
    } else {
        blk_set(b, bsz, 1);
    }
}

void *malloc(size_t size)
{
    if (size == 0) return NULL;

    uint32_t payload = (uint32_t)ALIGN_UP((uint32_t)size, ALIGNMENT);
    uint32_t total = payload + (uint32_t)sizeof(Block) + (uint32_t)sizeof(BlockFooter);
    total = (uint32_t)ALIGN_UP(total, ALIGNMENT);
    if (total < MIN_BLOCK_SIZE) total = MIN_BLOCK_SIZE;

    Block *b = find_fit(total);
    if (!b) {
        if (!heap_extend(total)) return NULL;
        b = find_fit(total);
        if (!b) return NULL;
    }

    split_and_alloc(b, total);
    return (void *)((uint8_t *)b + sizeof(Block));
}

static Block *coalesce(Block *b)
{
    // prev: guard against blk_prev_footer going out of bounds
    if ((uint32_t)b > _alloc_start + MIN_BLOCK_SIZE) {
        Block *p = blk_prev(b);
        if ((uint32_t)p >= _alloc_start && (uint32_t)p < _alloc_end && !blk_is_alloc(p)) {
            freelist_remove(p);
            uint32_t new_size = blk_size(p) + blk_size(b);
            b = p;
            blk_set(b, new_size, 0);
        }
    }

    // next: guard against blk_next going past the end of the heap
    Block *n = blk_next(b);
    if ((uint32_t)n >= _alloc_start && (uint32_t)n < _alloc_end && !blk_is_alloc(n)) {
        freelist_remove(n);
        uint32_t new_size = blk_size(b) + blk_size(n);
        blk_set(b, new_size, 0);
    }

    return b;
}


void free(void *ptr)
{
    if (!ptr) return;
    Block *b = (Block *)((uint8_t *)ptr - sizeof(Block));

    // basic sanity: block must be inside heap range
    if ((uint32_t)b < _alloc_start || (uint32_t)b >= _alloc_end) {
        return;
    }

    blk_set(b, blk_size(b), 0);
    b = coalesce(b);
    freelist_insert(b);
}


// void *malloc(size_t size)
// {
//   int res = size % PAGE_SIZE;
//   int npages = size/PAGE_SIZE;

//   if (res>0) npages++;
//   return page_alloc(npages);
// }

// void free(void *p)
// {
// 	page_free(p);
// }

void page_test()
{
    void *a = malloc(1);
    void *b = malloc(2);
    void *c = malloc(3);
    kprintf("a=%x b=%x c=%x\n", a, b, c);

    free(b);
    void *d = malloc(2);
    kprintf("d=%x (should reuse b)\n", d);

    free(a);
    free(c);
    free(d);
}
