#include "pmm.h"
#include "../console/console.h"
#include "../libc/string.h"
#include "../io/serial.h"

//Bitmap to track page allocation status
//Each bit represents a 4KB page
// 0: free, 1: used
static uint8_t* page_bitmap;
static uint32_t total_pages;
static uint32_t used_pages;

//Helper, set a bit in bitmap
static inline void bitmap_set(const uint32_t bit) {
    page_bitmap[bit / 8] |= (1 << (bit % 8));
}

//Helper, Clear bit in bitmap
static inline void bitmap_clear(const uint32_t bit) {
    page_bitmap[bit / 8] &= ~(1 << (bit % 8));
}

//Helper, test bit in bitmap
static inline int bitmap_test(uint32_t bit) {
    return page_bitmap[bit / 8] & (1 << (bit % 8));
}

//Convert physical address to page index
static inline uint32_t addr_to_page(uint64_t addr) {
    return (addr - PHYS_FREE_START) / PAGE_SIZE;
}

// Convert page index to physical address
static inline uint64_t page_to_addr(uint32_t page) {
    return PHYS_FREE_START + (page * PAGE_SIZE);
}

kerr_t pmm_init(void) {
    // Calculate number of pages in memory
    total_pages = (PHYS_MEMORY_END - PHYS_FREE_START) / PAGE_SIZE;

    //Calculate bitmap size
    size_t bitmap_size = (total_pages + 7) / 8;

    serial_debug_puts("[PMM] Total Pages: ");
    char num_str[32];
    uitoa(total_pages, num_str);
    serial_debug_puts(num_str);
    serial_debug_putc('\n');

    serial_debug_puts("[PMM] Bitmap Size: ");
    uitoa(bitmap_size, num_str);
    serial_debug_puts(num_str);
    serial_debug_putc('\n');

    //Place bitmap at PHYS_BITMAP_START
    page_bitmap = (uint8_t*)PHYS_BITMAP_START;

    //Mark all pages as free
    memset(page_bitmap, 0, bitmap_size);
    used_pages = 0;

    //Mark used regions
    //1: low memory 0-1MB
    pmm_mark_region_used(PHYS_LOW_MEM_START, PHYS_LOW_MEM_END);

    //2: Kernel memory 1-2MB
    pmm_mark_region_used(PHYS_KERNEL_START, PHYS_KERNEL_END);

    //3: Initial heap 2-3MB
    pmm_mark_region_used(PHYS_HEAP_START, PHYS_HEAP_END);

    // 4. Bitmap itself (3MB-4MB)
    pmm_mark_region_used(PHYS_BITMAP_START, PHYS_BITMAP_END);

    serial_debug_puts("[PMM] Initialization complete\n");
    serial_debug_puts("[PMM] Free memory: ");
    uitoa(pmm_get_free_memory() / 1024 / 1024, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts(" MB\n");
}