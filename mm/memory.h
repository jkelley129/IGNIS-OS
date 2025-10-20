#ifndef MEMORY_H
#define MEMORY_H

#include "../libc/stdint.h"
#include "../libc/stddef.h"
#include "error_handling/errno.h"

// Memory allocation block header
typedef struct memory_block {
    size_t size;
    uint8_t is_free;
    struct memory_block* next;
} memory_block_t;

#define MEMORY_BLOCK_HEADER_SIZE sizeof(memory_block_t)

// Opaque heap structure
typedef struct heap heap_t;

// Memory allocator functions
kerr_t memory_init(uint64_t heap_start, uint64_t heap_size);
heap_t* memory_get_kernel_heap(void);  // Get the kernel's heap
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);

// NEW: Page-aligned allocation helpers
void* kalloc_pages(size_t num_pages);
void kfree_pages(void* ptr, size_t num_pages);

// Memory utility functions
void memory_print_stats(void);
uint64_t memory_get_free(void);
uint64_t memory_get_used(void);
uint64_t memory_get_total(void);

#endif