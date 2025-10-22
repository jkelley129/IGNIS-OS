#include "memory.h"
#include "pmm.h"
#include "memory_layout.h"
#include "../console/console.h"
#include "../libc/string.h"
#include "error_handling/errno.h"

// Full definition of heap structure (hidden from other files)
struct heap {
    uint64_t start;
    uint64_t end;
    uint64_t current;
    memory_block_t* free_list;
};

// Global kernel heap - initialized once at boot
static heap_t* kernel_heap = NULL;

// Static helper functions (internal only)
static memory_block_t* find_free_block(heap_t* heap, size_t size) {
    memory_block_t* current = heap->free_list;

    while (current) {
        if (current->is_free && current->size >= size) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

static void coalesce_free_blocks(heap_t* heap) {
    memory_block_t* current = heap->free_list;
    while (current && current->next) {
        if (current->is_free && current->next->is_free) {
            current->size += current->next->size + MEMORY_BLOCK_HEADER_SIZE;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

kerr_t memory_init(uint64_t start, uint64_t size) {
    // Allocate heap structure from the heap memory itself
    kernel_heap = (heap_t*)start;
    kernel_heap->start = start + sizeof(heap_t);
    kernel_heap->end = start + size;
    kernel_heap->current = kernel_heap->start;
    kernel_heap->free_list = NULL;

    console_puts("Memory initialized");
    char addr_str[32];
    console_puts(" with size ");
    uitoa(size / 1024, addr_str);
    console_puts(addr_str);
    console_puts(" KB   ");

    return E_OK;
}

heap_t* memory_get_kernel_heap(void) {
    return kernel_heap;
}

// NEW: Page-aligned allocation helpers
void* kalloc_pages(size_t num_pages) {
    uint64_t phys = pmm_alloc_pages(num_pages);
    if (!phys) return NULL;
    return PHYS_TO_VIRT(phys);
}

uint64_t memory_get_free(void) {
    if (!kernel_heap) return 0;
    return kernel_heap->end - kernel_heap->current;
}

uint64_t memory_get_used(void) {
    if (!kernel_heap) return 0;
    return kernel_heap->current - kernel_heap->start;
}

uint64_t memory_get_total(void) {
    if (!kernel_heap) return 0;
    return kernel_heap->end - kernel_heap->start;
}

void memory_print_stats(void) {
    if (!kernel_heap) return;

    console_puts("\n=== Memory Statistics ===\n");

    char num_str[32];
    uint64_t total_size = kernel_heap->end - kernel_heap->start;
    uint64_t used_size = kernel_heap->current - kernel_heap->start;
    uint64_t free_size = kernel_heap->end - kernel_heap->current;

    console_puts("Total heap: ");
    uitoa(total_size / 1024, num_str);
    console_puts(num_str);
    console_puts(" KB\n");

    console_puts("Used: ");
    uitoa(used_size, num_str);
    console_puts(num_str);
    console_puts(" bytes\n");

    console_puts("Free: ");
    uitoa(free_size / 1024, num_str);
    console_puts(num_str);
    console_puts(" KB\n");

    // Count blocks
    uint64_t total_blocks = 0;
    uint64_t free_blocks = 0;
    memory_block_t* current = kernel_heap->free_list;

    while (current) {
        total_blocks++;
        if (current->is_free) {
            free_blocks++;
        }
        current = current->next;
    }

    console_puts("Total blocks: ");
    uitoa(total_blocks, num_str);
    console_puts(num_str);
    console_puts("\n");

    console_puts("Free blocks: ");
    uitoa(free_blocks, num_str);
    console_puts(num_str);
    console_puts("\n\n");
}