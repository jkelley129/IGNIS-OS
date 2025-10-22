#include "buddy.h"
#include "../console/console.h"
#include "../io/serial.h"
#include "../libc/string.h"

//Global buddy allocator instance
static buddy_allocator_t* g_buddy_allocator = NULL;

//Helper: get index from block address
static inline uint64_t addr_to_index(buddy_allocator_t* alloc, uint64_t addr) {
    return (addr - alloc->base_addr) / PAGE_SIZE;
}

//Helper: get address from block index
static inline uint64_t index_to_addr(buddy_allocator_t* alloc, uint64_t index) {
    return alloc->base_addr (index * PAGE_SIZE);
}

//Helper: calculate buddy address
static inline uint64_t get_buddy_addr(buddy_allocator_t* alloc, const uint64_t addr, const uint8_t order) {
    uint64_t block_index = addr_to_index(alloc, addr);
    uint64_t buddy_index = block_index ^ BUDDY_PAGES_PER_ORDER(order);
    return index_to_addr(alloc, buddy_index);
}

// Helper: Set bit in allocation bitmap
static inline void bitmap_set(uint8_t* bitmap, uint64_t bit) {
    bitmap[bit / 8] |= (1 << (bit % 8));
}

// Helper: Clear bit in allocation bitmap
static inline void bitmap_clear(uint8_t* bitmap, uint64_t bit) {
    bitmap[bit / 8] &= ~(1 << (bit % 8));
}

// Helper: Test bit in allocation bitmap
static inline int bitmap_test(uint8_t* bitmap, uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

static void remove_from_tree_list(buddy_allocator_t* alloc, buddy_block_t* block, uint8_t order) {
    if (block->prev) block->prev->next = block->next;
    else alloc->free_lists[order] = block->next;

    if (block->next) block->next->prev = block->prev;

    block->next = NULL;
    block->prev = NULL;
}

// Add block to free list
static void add_to_free_list(buddy_allocator_t* alloc, uint64_t addr, uint8_t order) {
    buddy_block_t* block = (buddy_block_t*)PHYS_TO_VIRT(addr);

    block->next = alloc->free_lists[order];
    block->prev = NULL;

    if (alloc->free_lists[order]) {
        alloc->free_lists[order]->prev = block;
    }

    alloc->free_lists[order] = block;
}

// Split a block into two smaller blocks
static kerr_t split_block(buddy_allocator_t* alloc, uint8_t order) {
    if (order >= BUDDY_MAX_ORDER) {
        return E_INVALID;
    }

    // Need a free block in the next larger order
    if (!alloc->free_lists[order + 1]) {
        // Recursively split larger block
        kerr_t err = split_block(alloc, order + 1);
        if (err != E_OK) {
            return err;
        }
    }

    // Take block from larger order
    buddy_block_t* block = alloc->free_lists[order + 1];
    uint64_t block_addr = VIRT_TO_PHYS((uint64_t)block);

    remove_from_free_list(alloc, block, order + 1);

    // Split into two buddies
    uint64_t buddy1_addr = block_addr;
    uint64_t buddy2_addr = block_addr + BUDDY_SIZE_FOR_ORDER(order);

    // Add both to free list of smaller order
    add_to_free_list(alloc, buddy1_addr, order);
    add_to_free_list(alloc, buddy2_addr, order);

    alloc->splits++;

    return E_OK;
}

// Try to merge block with its buddy
static void try_merge(buddy_allocator_t* alloc, uint64_t addr, uint8_t order) {
    if (order >= BUDDY_MAX_ORDER) {
        return;
    }

    uint64_t buddy_addr = get_buddy_addr(alloc, addr, order);

    // Check if buddy is in range
    if (buddy_addr < alloc->base_addr ||
        buddy_addr >= alloc->base_addr + alloc->total_size) {
        return;
    }

    // Check if buddy is free
    uint64_t buddy_index = addr_to_block_index(alloc, buddy_addr);
    if (bitmap_test(alloc->allocation_bitmap, buddy_index)) {
        return; // Buddy is allocated
    }

    // Find buddy in free list
    buddy_block_t* buddy = alloc->free_lists[order];
    buddy_block_t* buddy_virt = (buddy_block_t*)PHYS_TO_VIRT(buddy_addr);

    while (buddy) {
        if (buddy == buddy_virt) {
            // Found buddy in free list - merge!
            remove_from_free_list(alloc, buddy, order);

            // Remove current block from free list
            buddy_block_t* current = (buddy_block_t*)PHYS_TO_VIRT(addr);
            remove_from_free_list(alloc, current, order);

            // Merged block starts at lower address
            uint64_t merged_addr = (addr < buddy_addr) ? addr : buddy_addr;

            // Add merged block to next order
            add_to_free_list(alloc, merged_addr, order + 1);

            alloc->merges++;

            // Recursively try to merge at higher order
            try_merge(alloc, merged_addr, order + 1);

            return;
        }
        buddy = buddy->next;
    }
}

kerr_t buddy_init(buddy_allocator_t* allocator, uint64_t base_addr, uint64_t size) {
    if (!allocator || !IS_PAGE_ALIGNED(base_addr) || !IS_PAGE_ALIGNED(size)) {
        return E_INVALID;
    }

    // Size must be power of 2
    if ((size & (size - 1)) != 0) {
        return E_INVALID;
    }

    allocator->base_addr = base_addr;
    allocator->total_size = size;
    allocator->total_pages = size / PAGE_SIZE;

    // Initialize free lists
    for (int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        allocator->free_lists[i] = NULL;
        allocator->allocations[i] = 0;
        allocator->deallocations[i] = 0;
    }

    allocator->splits = 0;
    allocator->merges = 0;

    // Calculate bitmap size (1 bit per page)
    size_t bitmap_size = (allocator->total_pages + 7) / 8;

    // Allocate bitmap at start of managed region
    allocator->allocation_bitmap = (uint8_t*)PHYS_TO_VIRT(base_addr);
    memset(allocator->allocation_bitmap, 0, bitmap_size);

    // Mark bitmap pages as used
    size_t bitmap_pages = (bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
    for (size_t i = 0; i < bitmap_pages; i++) {
        bitmap_set(allocator->allocation_bitmap, i);
    }

    // Add remaining memory to largest possible orders
    uint64_t current_addr = base_addr + (bitmap_pages * PAGE_SIZE);
    uint64_t remaining_size = size - (bitmap_pages * PAGE_SIZE);

    // Add blocks in descending order of size
    for (int order = BUDDY_MAX_ORDER; order >= 0; order--) {
        size_t block_size = BUDDY_SIZE_FOR_ORDER(order);

        while (remaining_size >= block_size) {
            add_to_free_list(allocator, current_addr, order);
            current_addr += block_size;
            remaining_size -= block_size;
        }
    }

    // Set global allocator
    g_buddy_allocator = allocator;

    serial_debug_puts("[BUDDY] Initialized at 0x");
    char addr_str[32];
    uitoa(base_addr, addr_str);
    serial_debug_puts(addr_str);
    serial_debug_puts(" with ");
    uitoa(size / 1024 / 1024, addr_str);
    serial_debug_puts(addr_str);
    serial_debug_puts(" MB\n");

    return E_OK;
}

uint8_t buddy_get_order_for_size(size_t size) {
    // Round up to page size
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

    // Find smallest order that fits
    uint8_t order = 0;
    while (BUDDY_PAGES_PER_ORDER(order) < pages && order < BUDDY_MAX_ORDER) {
        order++;
    }

    return order;
}

size_t buddy_get_actual_size(size_t size) {
    uint8_t order = buddy_get_order_for_size(size);
    return BUDDY_SIZE_FOR_ORDER(order);
}

uint64_t buddy_alloc_order(buddy_allocator_t* allocator, uint8_t order) {
    if (!allocator || order > BUDDY_MAX_ORDER) {
        return 0;
    }

    // Find free block of requested order
    if (!allocator->free_lists[order]) {
        // Need to split larger block
        kerr_t err = split_block(allocator, order);
        if (err != E_OK) {
            return 0;
        }
    }

    // Take first block from free list
    buddy_block_t* block = allocator->free_lists[order];
    if (!block) {
        return 0;
    }

    uint64_t addr = VIRT_TO_PHYS((uint64_t)block);

    remove_from_free_list(allocator, block, order);

    // Mark pages as allocated
    uint64_t block_index = addr_to_block_index(allocator, addr);
    size_t num_pages = BUDDY_PAGES_PER_ORDER(order);
    for (size_t i = 0; i < num_pages; i++) {
        bitmap_set(allocator->allocation_bitmap, block_index + i);
    }

    allocator->allocations[order]++;

    return addr;
}

uint64_t buddy_alloc(buddy_allocator_t* allocator, size_t size) {
    uint8_t order = buddy_get_order_for_size(size);
    return buddy_alloc_order(allocator, order);
}

void buddy_free(buddy_allocator_t* allocator, uint64_t phys_addr) {
    if (!allocator || phys_addr < allocator->base_addr ||
        phys_addr >= allocator->base_addr + allocator->total_size) {
        return;
    }

    if (!IS_PAGE_ALIGNED(phys_addr)) {
        serial_debug_puts("[BUDDY] Warning: Freeing non-aligned address\n");
        return;
    }

    // Find order of this block by checking bitmap
    uint64_t block_index = addr_to_block_index(allocator, phys_addr);

    if (!bitmap_test(allocator->allocation_bitmap, block_index)) {
        serial_debug_puts("[BUDDY] Warning: Double free detected\n");
        return;
    }

    // Determine order by checking contiguous allocated pages
    uint8_t order = 0;
    size_t num_pages = 1;

    while (order < BUDDY_MAX_ORDER) {
        size_t pages_for_next_order = BUDDY_PAGES_PER_ORDER(order + 1);
        int all_allocated = 1;

        for (size_t i = num_pages; i < pages_for_next_order; i++) {
            if (!bitmap_test(allocator->allocation_bitmap, block_index + i)) {
                all_allocated = 0;
                break;
            }
        }

        if (!all_allocated) {
            break;
        }

        order++;
        num_pages = pages_for_next_order;
    }

    // Mark pages as free
    for (size_t i = 0; i < num_pages; i++) {
        bitmap_clear(allocator->allocation_bitmap, block_index + i);
    }

    allocator->deallocations[order]++;

    // Add to free list
    add_to_free_list(allocator, phys_addr, order);

    // Try to merge with buddy
    try_merge(allocator, phys_addr, order);
}

int buddy_is_allocated(buddy_allocator_t* allocator, uint64_t phys_addr) {
    if (!allocator || phys_addr < allocator->base_addr ||
        phys_addr >= allocator->base_addr + allocator->total_size) {
        return 0;
    }

    uint64_t block_index = addr_to_block_index(allocator, phys_addr);
    return bitmap_test(allocator->allocation_bitmap, block_index);
}

uint64_t buddy_get_free_memory(buddy_allocator_t* allocator) {
    if (!allocator) return 0;

    uint64_t free_mem = 0;
    for (int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        buddy_block_t* block = allocator->free_lists[order];
        int count = 0;

        while (block) {
            count++;
            block = block->next;
        }

        free_mem += count * BUDDY_SIZE_FOR_ORDER(order);
    }

    return free_mem;
}

uint64_t buddy_get_used_memory(buddy_allocator_t* allocator) {
    if (!allocator) return 0;
    return allocator->total_size - buddy_get_free_memory(allocator);
}

void buddy_print_stats(buddy_allocator_t* allocator) {
    if (!allocator) return;

    console_puts("\n=== Buddy Allocator Statistics ===\n");

    char num_str[32];

    console_puts("Total memory: ");
    uitoa(allocator->total_size / 1024 / 1024, num_str);
    console_puts(num_str);
    console_puts(" MB\n");

    console_puts("Used memory:  ");
    uitoa(buddy_get_used_memory(allocator) / 1024, num_str);
    console_puts(num_str);
    console_puts(" KB\n");

    console_puts("Free memory:  ");
    uitoa(buddy_get_free_memory(allocator) / 1024, num_str);
    console_puts(num_str);
    console_puts(" KB\n\n");

    console_puts("Splits: ");
    uitoa(allocator->splits, num_str);
    console_puts(num_str);
    console_puts("  Merges: ");
    uitoa(allocator->merges, num_str);
    console_puts(num_str);
    console_puts("\n\n");

    console_puts("Free blocks by order:\n");
    for (int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        int count = 0;
        buddy_block_t* block = allocator->free_lists[order];

        while (block) {
            count++;
            block = block->next;
        }

        if (count > 0) {
            console_puts("  Order ");
            uitoa(order, num_str);
            console_puts(num_str);
            console_puts(" (");
            uitoa(BUDDY_SIZE_FOR_ORDER(order) / 1024, num_str);
            console_puts(num_str);
            console_puts(" KB): ");
            uitoa(count, num_str);
            console_puts(num_str);
            console_puts(" blocks\n");
        }
    }

    console_putc('\n');
}

buddy_allocator_t* buddy_get_global(void) {
    return g_buddy_allocator;
}
