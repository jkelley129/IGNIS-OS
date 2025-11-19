#ifndef PMM_H
#define PMM_H

#include "libc/stdint.h"
#include "libc/stddef.h"
#include "error_handling/errno.h"
#include "memory_layout.h"

// Physical memory manager - tracks free/used 4KB page frames

// Initialize the physical memory manager
// Must be called early in boot with multiboot memory map
kerr_t pmm_init(void);

// Allocate a single 4KB physical page frame
// Returns physical address of the page, or 0 on failure
uint64_t pmm_alloc_page(void);

// Free a single 4KB physical page frame
// addr must be page-aligned physical address
void pmm_free_page(uint64_t phys_addr);

// Allocate multiple contiguous pages
// Returns physical address of first page, or 0 on failure
uint64_t pmm_alloc_pages(size_t count);

// Free multiple contiguous pages
void pmm_free_pages(uint64_t phys_addr, size_t count);

// Mark a physical memory region as used (for kernel, hardware, etc.)
void pmm_mark_region_used(uint64_t start, uint64_t end);

// Mark a physical memory region as free
void pmm_mark_region_free(uint64_t start, uint64_t end);

// Get memory statistics
uint64_t pmm_get_total_memory(void);
uint64_t pmm_get_used_memory(void);
uint64_t pmm_get_free_memory(void);
uint32_t pmm_get_total_pages(void);
uint32_t pmm_get_used_pages(void);
uint32_t pmm_get_free_pages(void);

// Print memory map (for debugging)
void pmm_print_stats(void);

#endif