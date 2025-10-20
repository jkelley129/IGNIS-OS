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

uint64_t pmm_alloc_page(void) {
    //Find first free page
    for (size_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
            return page_to_addr(i);
        }
    }

    //Out of memory(would return E_NOMEM but return type must be uint64_t)
    return 0;
}

void pmm_free_page(uint64_t phys_addr) {
    if (phys_addr < PHYS_LOW_MEM_START || phys_addr >= PHYS_HEAP_END) return;

    if (!IS_PAGE_ALIGNED(phys_addr)) return;

    uint32_t page = addr_to_page(phys_addr);
    if (page >= total_pages) return;

    if (bitmap_test(page)) {
        bitmap_clear(page);
        used_pages--;
    }
}

uint64_t pmm_alloc_pages(size_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();

    //Find contiguous free pages
    uint32_t contiguous = 0;
    uint32_t start = 0;

    for (uint32_t i = 0; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (contiguous == 0) start = i;
            contiguous++;

            if (contiguous == count) {
                //Found enough pages
                for (uint32_t j = start; j < start + count; j++) {
                    bitmap_set(j);
                }

                used_pages+=count;
                return page_to_addr(start);
            }
        }else {
            contiguous = 0;
        }
    }

    return 0; //No pages
}

void pmm_free_pages(uint64_t phys_addr, size_t count) {
    for (size_t i = 0; i < count; i++) {
        pmm_free_page(phys_addr + (i + PAGE_SIZE));
    }
}

void pmm_mark_region_used(uint64_t start, uint64_t end) {
    //Align to boundries
    start = PAGE_ALIGN_DOWN(start);
    end = PAGE_ALIGN_UP(end);

    //Only mark pages in region
    if (start < PHYS_FREE_START) start = PHYS_FREE_START;
    if (end > PHYS_MEMORY_END) start = PHYS_MEMORY_END;
    if (start >= end) return;

    uint32_t start_page = addr_to_page(start);
    uint32_t end_page = addr_to_page(end);

    for (uint32_t i = start_page; i < end_page && i < total_pages; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            used_pages++;
        }
    }
}

void pmm_mark_region_free(uint64_t start, uint64_t end) {
    start = PAGE_ALIGN_DOWN(start);
    end = PAGE_ALIGN_UP(end);
    if (start < PHYS_FREE_START) start = PHYS_FREE_START;
    if (end > PHYS_MEMORY_END) start = PHYS_MEMORY_END;
    if (start >= end) return;

    uint32_t start_page = addr_to_page(start);
    uint32_t end_page = addr_to_page(end);

    for (uint32_t i = start_page; i < end_page && i < total_pages; i++) {
        if (bitmap_test(i)) {
            bitmap_clear(i);
            used_pages--;
        }
    }
}

uint32_t pmm_get_total_pages(void) {
    return total_pages;
}

uint32_t pmm_get_used_pages(void) {
    return used_pages;
}

uint32_t pmm_get_free_pages(void) {
    return total_pages - used_pages;
}

uint64_t pmm_get_total_memory(void) {
    return (uint64_t)total_pages * PAGE_SIZE;
}

uint64_t pmm_get_used_memory(void) {
    return (uint64_t)used_pages * PAGE_SIZE;
}

uint64_t pmm_get_free_memory(void) {
    return (uint64_t)pmm_get_free_pages() * PAGE_SIZE;
}

void pmm_print_stats(void) {
    console_puts("\n=== Physical Memory Manager ===\n");

    char num_str[32];

    console_puts("Total memory: ");
    uitoa(pmm_get_total_memory() / 1024 / 1024, num_str);
    console_puts(num_str);
    console_puts(" MB (");
    uitoa(total_pages, num_str);
    console_puts(num_str);
    console_puts(" pages)\n");

    console_puts("Used memory:  ");
    uitoa(pmm_get_used_memory() / 1024 / 1024, num_str);
    console_puts(num_str);
    console_puts(" MB (");
    uitoa(used_pages, num_str);
    console_puts(num_str);
    console_puts(" pages)\n");

    console_puts("Free memory:  ");
    uitoa(pmm_get_free_memory() / 1024 / 1024, num_str);
    console_puts(num_str);
    console_puts(" MB (");
    uitoa(pmm_get_free_pages(), num_str);
    console_puts(num_str);
    console_puts(" pages)\n");

    console_puts("Page size:    ");
    uitoa(PAGE_SIZE, num_str);
    console_puts(num_str);
    console_puts(" bytes\n\n");
}