#include "kmalloc.h"
#include "slab.h"
#include "buddy.h"
#include "../pmm.h"
#include "../memory_layout.h"
#include "../../console/console.h"
#include "../../libc/string.h"

/*
 * - Requests ≤ 4KB go to slab allocator
 * - Requests > 4KB go to buddy allocator (page-level allocation)
 * - Slab allocator uses buddy allocator internally for getting pages
 */

// Magic number to identify buddy-allocated blocks
// This helps kfree() determine which allocator was used
#define BUDDY_MAGIC 0xB0DD1E5

// Header for buddy allocations (stored before returned pointer)
typedef struct {
    uint32_t magic;     // BUDDY_MAGIC
    uint32_t order;     // Buddy order used
    uint64_t size;      // Original requested size
} buddy_alloc_header_t;

//========================================================

// Check if a pointer was allocated by buddy allocator
static inline int is_buddy_allocation(void* ptr) {
    if (!ptr) return 0;

    buddy_alloc_header_t* header = (buddy_alloc_header_t*)((uint64_t)ptr - sizeof(buddy_alloc_header_t));
    return header->magic == BUDDY_MAGIC;
}

// Get the order of a buddy allocation
static inline uint32_t get_buddy_order(void* ptr) {
    buddy_alloc_header_t* header = (buddy_alloc_header_t*)((uint64_t)ptr - sizeof(buddy_alloc_header_t));
    return header->order;
}

// Get the original size of a buddy allocation
static inline uint64_t get_buddy_size(void* ptr) {
    buddy_alloc_header_t* header = (buddy_alloc_header_t*)((uint64_t)ptr - sizeof(buddy_alloc_header_t));
    return header->size;
}

// ============================================================================

void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Small allocations go to slab allocator
    if (size <= 4096) {
        return slab_kmalloc(size);
    }

    // Large allocations go to buddy allocator
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return NULL;

    // We need extra space for header
    size_t total_size = size + sizeof(buddy_alloc_header_t);
    uint8_t order = buddy_get_order_for_size(total_size);

    uint64_t phys = buddy_alloc_order(buddy, order);
    if (!phys) return NULL;

    void* virt = PHYS_TO_VIRT(phys);

    // Write header
    buddy_alloc_header_t* header = (buddy_alloc_header_t*)virt;
    header->magic = BUDDY_MAGIC;
    header->order = order;
    header->size = size;

    // Return pointer after header
    return (void*)((uint64_t)virt + sizeof(buddy_alloc_header_t));
}

void kfree(void* ptr) {
    if (!ptr) return;

    // Check if this was a buddy allocation
    if (is_buddy_allocation(ptr)) {
        // Free via buddy allocator
        buddy_allocator_t* buddy = buddy_get_global();
        if (!buddy) return;

        // Get the actual allocation start (before header)
        void* alloc_start = (void*)((uint64_t)ptr - sizeof(buddy_alloc_header_t));
        uint64_t phys = VIRT_TO_PHYS((uint64_t)alloc_start);

        buddy_free(buddy, phys);
    } else {
        // Try slab allocator
        slab_kfree(ptr);
    }
}

void* kcalloc(size_t num, size_t size) {
    size_t total = num * size;

    // Check for overflow
    if (num != 0 && total / num != size) {
        return NULL;
    }

    void* ptr = kmalloc(total);

    if (ptr) {
        memset(ptr, 0, total);
    }

    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    // Get old size
    size_t old_size;

    if (is_buddy_allocation(ptr)) {
        old_size = get_buddy_size(ptr);
    } else {
        // For slab allocations, we need to check which cache it came from
        // This is a limitation - we'll just allocate new and copy
        // We don't know the exact old size, so we'll copy up to new_size
        old_size = new_size; // Conservative estimate
    }

    // If new size fits in same allocation, just return the same pointer
    if (new_size <= old_size) {
        return ptr;
    }

    // Allocate new block
    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return NULL;
    }

    // Copy old data
    size_t copy_size = (old_size < new_size) ? old_size : new_size;
    memcpy(new_ptr, ptr, copy_size);

    // Free old block
    kfree(ptr);

    return new_ptr;
}

// ============================================================================

void* kmalloc_pages(size_t num_pages) {
    if (num_pages == 0) return NULL;

    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return NULL;

    // Find order that fits num_pages
    uint8_t order = 0;
    size_t pages = 1;

    while (pages < num_pages && order < BUDDY_MAX_ORDER) {
        order++;
        pages = BUDDY_PAGES_PER_ORDER(order);
    }

    if (pages < num_pages) {
        return NULL; // Too many pages requested
    }

    uint64_t phys = buddy_alloc_order(buddy, order);
    if (!phys) return NULL;

    return PHYS_TO_VIRT(phys);
}

void kfree_pages(void* ptr, size_t num_pages) {
    if (!ptr) return;

    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return;

    uint64_t phys = VIRT_TO_PHYS((uint64_t)ptr);
    buddy_free(buddy, phys);
}

// ============================================================================

void kmalloc_print_stats(void) {
    console_puts("\n");
    console_puts("================================\n");
    console_puts("  KERNEL MEMORY STATISTICS\n");
    console_puts("================================\n");

    // Physical memory stats
    pmm_print_stats();

    // Buddy allocator stats
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        buddy_print_stats(buddy);
    }

    // Slab allocator stats
    slab_print_all_stats();
}

uint64_t kmalloc_get_used_memory(void) {
    uint64_t total = 0;

    // Add buddy allocator used memory
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        total += buddy_get_used_memory(buddy);
    }

    return total;
}

uint64_t kmalloc_get_free_memory(void) {
    uint64_t total = 0;

    // Add buddy allocator free memory
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        total += buddy_get_free_memory(buddy);
    }

    return total;
}