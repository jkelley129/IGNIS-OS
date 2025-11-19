#ifndef KMALLOC_H
#define KMALLOC_H

#include "libc/stdint.h"
#include "libc/stddef.h"

// Unified kernel memory allocation interface
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);

// Page-aligned allocations
void* kmalloc_pages(size_t num_pages);
void kfree_pages(void* ptr, size_t num_pages);

// Statistics
void kmalloc_print_stats(void);

#endif