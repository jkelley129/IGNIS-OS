#ifndef BUDDY_H
#define BUDDY_H

#include "libc/stdint.h"
#include "libc/stddef.h"
#include "error_handling/errno.h"
#include "mm/memory_layout.h"

// Maximum order (2^11 pages = 8MB blocks)
#define BUDDY_MAX_ORDER 11

// Minimum allocation is one page (order 0 = 4KB)
#define BUDDY_MIN_ORDER 0

// Calculate number of pages in an order
#define BUDDY_PAGES_PER_ORDER(order) (1ULL << (order))

// Calculate size in bytes for an order
#define BUDDY_SIZE_FOR_ORDER(order) (PAGE_SIZE * BUDDY_PAGES_PER_ORDER(order))

// Free list node (stored in free blocks)
typedef struct buddy_block {
    struct buddy_block* next;
    struct buddy_block* prev;
} buddy_block_t;

// Buddy allocator struct
typedef struct {
    uint64_t base_addr;
    uint64_t total_size;
    uint64_t total_pages;

    buddy_block_t* free_lists[BUDDY_MAX_ORDER + 1];

    uint64_t allocations[BUDDY_MAX_ORDER + 1];
    uint64_t deallocations[BUDDY_MAX_ORDER + 1];
    uint64_t splits;
    uint64_t merges;

    uint8_t* allocation_bitmap;
    uint8_t* order_bitmap;  // NEW: Store order for each allocation
} buddy_allocator_t;

// Initialize buddy allocator
// base_addr: Physical address of memory region to manage
// size: Size of region in bytes (must be power of 2)
kerr_t buddy_init(buddy_allocator_t* allocator, uint64_t base_addr, uint64_t size);

// Allocate memory block of at least 'size' bytes
// Returns physical address or 0 on failure
uint64_t buddy_alloc(buddy_allocator_t* allocator, size_t size);

// Allocate memory block of specific order
// order: log2(pages) - e.g., order 0 = 1 page (4KB), order 1 = 2 pages (8KB)
uint64_t buddy_alloc_order(buddy_allocator_t* allocator, uint8_t order);

// Free memory block at physical address
void buddy_free(buddy_allocator_t* allocator, uint64_t phys_addr);

// Get order for a given size
uint8_t buddy_get_order_for_size(size_t size);

// Get actual size allocated for a given size request
size_t buddy_get_actual_size(size_t size);

// Check if address is allocated
int buddy_is_allocated(buddy_allocator_t* allocator, uint64_t phys_addr);

// Statistics
uint64_t buddy_get_free_memory(buddy_allocator_t* allocator);
uint64_t buddy_get_used_memory(buddy_allocator_t* allocator);
void buddy_print_stats(buddy_allocator_t* allocator);

// Get global buddy allocator (initialized in buddy_init)
buddy_allocator_t* buddy_get_global(void);

#endif