#ifndef VMM_H
#define VMM_H

#include "libc/stdint.h"
#include "error_handling/errno.h"
#include "memory_layout.h"

// Virtual memory manager - handles page table manipulation

// Initialize VMM with current page tables
kerr_t vmm_init(void);

// Map a virtual page to a physical page with specified flags
kerr_t vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

// Unmap a virtual page
kerr_t vmm_unmap_page(uint64_t virt_addr);

// Get physical address for a virtual address
// Returns 0 if not mapped
uint64_t vmm_get_physical(uint64_t virt_addr);

// Check if a virtual address is mapped
int vmm_is_mapped(uint64_t virt_addr);

// Allocate and map a new page at virtual address
// Automatically allocates physical page from PMM
kerr_t vmm_alloc_page(uint64_t virt_addr, uint64_t flags);

// Unmap and free a page
kerr_t vmm_free_page(uint64_t virt_addr);

// Get current CR3 (page table root)
uint64_t vmm_get_cr3(void);

// Flush TLB for a specific page
void vmm_flush_tlb_page(uint64_t virt_addr);

// Flush entire TLB
void vmm_flush_tlb(void);

void page_fault_handler(uint64_t fault_addr, uint64_t error_code);

#endif