#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

#include "libc/stdint.h"

/*
 * IGNIS OS Memory Layout
 * ======================
 * 
 * Physical Memory:
 * ----------------
 * 0x00000000 - 0x000FFFFF : Low memory (BIOS, VGA buffer, etc.) [1MB]
 * 0x00100000 - 0x001FFFFF : Kernel code/data [1MB]
 * 0x00200000 - 0x002FFFFF : Initial heap [1MB]
 * 0x00300000 - 0x003FFFFF : Page frame bitmap [1MB]
 * 0x00400000 - 0x07FFFFFF : Free physical pages [124MB in 128MB system]
 *
 * Virtual Memory (After Higher-Half Transition):
 * ----------------------------------------------
 * 0x0000000000000000 - 0x00007FFFFFFFFFFF : User space (128TB) [FUTURE]
 * 0xFFFF800000000000 - 0xFFFF807FFFFFFFFF : Physical memory direct map (512GB)
 * 0xFFFFFFFF80000000 - 0xFFFFFFFF9FFFFFFF : Kernel code/data (512MB)
 * 0xFFFFFFFFA0000000 - 0xFFFFFFFFBFFFFFFF : Kernel heap (512MB)
 * 0xFFFFFFFFC0000000 - 0xFFFFFFFFDFFFFFFF : Kernel stacks (512MB)
 * 0xFFFFFFFFE0000000 - 0xFFFFFFFFFFFFFFFF : Reserved (512MB)
 */

// ============================================================================
// Physical Memory Layout
// ============================================================================

// Reserved/special regions
#define PHYS_LOW_MEM_START      0x00000000ULL
#define PHYS_LOW_MEM_END        0x00100000ULL  // 1MB

// Kernel physical location (loaded by bootloader at 1MB)
#define PHYS_KERNEL_START       0x00100000ULL  // 1MB
#define PHYS_KERNEL_END         0x00200000ULL  // 2MB

// Initial heap (before paging is set up)
#define PHYS_HEAP_START         0x00200000ULL  // 2MB
#define PHYS_HEAP_END           0x00300000ULL  // 3MB
#define PHYS_HEAP_SIZE          (PHYS_HEAP_END - PHYS_HEAP_START)

// Page frame bitmap location
#define PHYS_BITMAP_START       0x00300000ULL  // 3MB
#define PHYS_BITMAP_END         0x00400000ULL  // 4MB
#define PHYS_BITMAP_SIZE        (PHYS_BITMAP_END - PHYS_BITMAP_START)

// Free physical memory starts here
#define PHYS_FREE_START         0x00400000ULL  // 4MB
#define PHYS_MEMORY_END         0x08000000ULL  // 128MB (default QEMU)

// ============================================================================
// Virtual Memory Layout (Higher-Half Kernel)
// ============================================================================

// User space (lower half) - Reserved for future use
#define VIRT_USER_START         0x0000000000000000ULL
#define VIRT_USER_END           0x00007FFFFFFFFFFFULL

// Physical memory direct mapping (offset mapping)
// Add this offset to physical address to get virtual address
#define VIRT_PHYS_MAP_BASE      0xFFFF800000000000ULL
#define VIRT_PHYS_MAP_SIZE      0x0000008000000000ULL  // 512GB

// Kernel code/data in higher half
#define VIRT_KERNEL_BASE        0xFFFFFFFF80000000ULL
#define VIRT_KERNEL_SIZE        0x0000000020000000ULL  // 512MB

// Kernel heap region
#define VIRT_HEAP_BASE          0xFFFFFFFFA0000000ULL
#define VIRT_HEAP_SIZE          0x0000000020000000ULL  // 512MB

// Kernel stacks region
#define VIRT_STACK_BASE         0xFFFFFFFFC0000000ULL
#define VIRT_STACK_SIZE         0x0000000020000000ULL  // 512MB

// Reserved region
#define VIRT_RESERVED_BASE      0xFFFFFFFFE0000000ULL
#define VIRT_RESERVED_SIZE      0x0000000020000000ULL  // 512MB

// ============================================================================
// Page and Frame Definitions
// ============================================================================

#define PAGE_SIZE               4096
#define PAGE_SHIFT              12
#define PAGE_MASK               (~(PAGE_SIZE - 1))

// Total number of 4KB pages in physical memory
#define TOTAL_PAGES             ((PHYS_MEMORY_END - PHYS_FREE_START) / PAGE_SIZE)

// Align address down to page boundary
#define PAGE_ALIGN_DOWN(addr)   ((addr) & PAGE_MASK)

// Align address up to page boundary
#define PAGE_ALIGN_UP(addr)     (((addr) + PAGE_SIZE - 1) & PAGE_MASK)

// Check if address is page-aligned
#define IS_PAGE_ALIGNED(addr)   (((addr) & (PAGE_SIZE - 1)) == 0)

// ============================================================================
// Conversion Macros (for after higher-half transition)
// ============================================================================

// Convert physical address to virtual address (direct map)
#define PHYS_TO_VIRT(phys)      ((void*)((uint64_t)(phys) + VIRT_PHYS_MAP_BASE))

// Convert virtual address to physical address (direct map region only)
#define VIRT_TO_PHYS(virt)      ((uint64_t)(virt) - VIRT_PHYS_MAP_BASE)

// Check if virtual address is in direct map region
#define IS_DIRECT_MAP(virt)     ((uint64_t)(virt) >= VIRT_PHYS_MAP_BASE && \
                                 (uint64_t)(virt) < (VIRT_PHYS_MAP_BASE + VIRT_PHYS_MAP_SIZE))

// Check if virtual address is in kernel region
#define IS_KERNEL_ADDR(virt)    ((uint64_t)(virt) >= VIRT_KERNEL_BASE)

// ============================================================================
// Page Table Entry Flags
// ============================================================================

#define PAGE_PRESENT            (1ULL << 0)   // Page is present in memory
#define PAGE_WRITE              (1ULL << 1)   // Page is writable
#define PAGE_USER               (1ULL << 2)   // Page is accessible from user mode
#define PAGE_WRITE_THROUGH      (1ULL << 3)   // Write-through caching
#define PAGE_CACHE_DISABLE      (1ULL << 4)   // Disable caching for this page
#define PAGE_ACCESSED           (1ULL << 5)   // Page has been accessed
#define PAGE_DIRTY              (1ULL << 6)   // Page has been written to
#define PAGE_HUGE               (1ULL << 7)   // 2MB page (in PD) or 1GB page (in PDPT)
#define PAGE_GLOBAL             (1ULL << 8)   // Global page (not flushed on CR3 change)
#define PAGE_NO_EXECUTE         (1ULL << 63)  // Execute disable

// Common flag combinations
#define PAGE_KERNEL_RO          (PAGE_PRESENT)
#define PAGE_KERNEL_RW          (PAGE_PRESENT | PAGE_WRITE)
#define PAGE_USER_RO            (PAGE_PRESENT | PAGE_USER)
#define PAGE_USER_RW            (PAGE_PRESENT | PAGE_WRITE | PAGE_USER)

// ============================================================================
// Helper Functions
// ============================================================================

// Get page table entry address from flags
static inline uint64_t pte_get_address(uint64_t pte) {
    return pte & 0x000FFFFFFFFFF000ULL;
}

// Create page table entry from address and flags
static inline uint64_t pte_create(uint64_t phys_addr, uint64_t flags) {
    return (phys_addr & 0x000FFFFFFFFFF000ULL) | (flags & 0xFFF);
}

// Check if page table entry has a flag
static inline int pte_has_flag(uint64_t pte, uint64_t flag) {
    return (pte & flag) != 0;
}

#endif