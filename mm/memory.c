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

void* kmalloc(size_t size) {
    if (!kernel_heap) return NULL;
    if (size == 0) return NULL;

    size = (size + 7) & ~7;  // Align to 8 bytes

    // Try to find free block
    memory_block_t* block = find_free_block(kernel_heap, size);
    memory_block_t* prev = NULL;
    memory_block_t* current = kernel_heap->free_list;

    while (current) {
        if (current->is_free && current->size >= size) {
            current->is_free = 0;

            // Split block if large enough
            if (current->size >= size + MEMORY_BLOCK_HEADER_SIZE + 16) {
                memory_block_t* new_block = (memory_block_t*)((uint64_t)current + MEMORY_BLOCK_HEADER_SIZE + size);
                new_block->size = current->size - size - MEMORY_BLOCK_HEADER_SIZE;
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            return (void*)((uint64_t)current + MEMORY_BLOCK_HEADER_SIZE);
        }
        prev = current;
        current = current->next;
    }

    // Need to allocate new block
    uint64_t block_addr = kernel_heap->current;
    uint64_t total_size = size + MEMORY_BLOCK_HEADER_SIZE;

    if (kernel_heap->current + total_size > kernel_heap->end) {
        console_puts("[MEMORY ERROR]: Out of memory!\n");
        return NULL;
    }

    memory_block_t* new_block = (memory_block_t*)block_addr;
    new_block->size = size;
    new_block->is_free = 0;
    new_block->next = NULL;

    if (kernel_heap->free_list == NULL) {
        kernel_heap->free_list = new_block;
    } else {
        memory_block_t* last = kernel_heap->free_list;
        while (last->next) {
            last = last->next;
        }
        last->next = new_block;
    }

    kernel_heap->current += total_size;
    return (void*)(block_addr + MEMORY_BLOCK_HEADER_SIZE);
}

void kfree(void* ptr) {
    if (!ptr || !kernel_heap) return;

    memory_block_t* block = (memory_block_t*)((uint64_t)ptr - MEMORY_BLOCK_HEADER_SIZE);
    block->is_free = 1;

    coalesce_free_blocks(kernel_heap);
}

void* kcalloc(size_t num, size_t size) {
    size_t total_size = num * size;
    void* ptr = kmalloc(total_size);

    if (ptr) {
        memset(ptr, 0, total_size);
    }

    return ptr;
}

void* krealloc(void* ptr, size_t new_size) {
    if (!ptr) return kmalloc(new_size);

    if (new_size == 0) {
        kfree(ptr);
        return NULL;
    }

    memory_block_t* block = (memory_block_t*)((uint64_t)ptr - MEMORY_BLOCK_HEADER_SIZE);

    if (block->size >= new_size) {
        return ptr;
    }

    void* new_ptr = kmalloc(new_size);
    if (!new_ptr) return NULL;

    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for (size_t i = 0; i < block->size; i++) {
        dst[i] = src[i];
    }

    kfree(ptr);
    return new_ptr;
}

// NEW: Page-aligned allocation helpers
void* kalloc_pages(size_t num_pages) {
    uint64_t phys = pmm_alloc_pages(num_pages);
    if (!phys) return NULL;
    return PHYS_TO_VIRT(phys);
}

void kfree_pages(void* ptr, size_t num_pages) {
    if (!ptr) return;
    uint64_t phys = VIRT_TO_PHYS((uint64_t)ptr);
    pmm_free_pages(phys, num_pages);
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