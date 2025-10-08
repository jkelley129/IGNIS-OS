#include "memory.h"
#include "../io/vga.h"
#include "../libc/string.h"


static uint64_t heap_start = 0;
static uint64_t heap_end = 0;
static uint64_t heap_current = 0;
static memory_block_t* free_list = 0;

void memory_init(uint64_t start, uint64_t size){
    heap_start = start;
    heap_end = start + size;
    heap_current = start;
    free_list = 0;

    vga_puts("Memory initialized");
    char addr_str[32];
    vga_puts(" with size ");
    uitoa(size / 1024, addr_str);
    vga_puts(addr_str);
    vga_puts(" KB   ");
}

void* kmalloc(size_t size){
    if(size == 0){
        return 0;
    }

    size = (size + 7) & ~7;

    memory_block_t* current = free_list;
    memory_block_t* prev = 0;

    while(current){
        if(current->is_free && current->size >= size){
            current->is_free = 0;

            if(current->size >= size + MEMORY_BLOCK_HEADER_SIZE + 16){
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

    uint64_t block_addr = heap_current;
    uint64_t total_size = size + MEMORY_BLOCK_HEADER_SIZE;
    if(heap_current + total_size > heap_end){
        vga_puts("[MEMORY ERROR]:Out of memory!\n");
        return 0;
    }

    memory_block_t* block = (memory_block_t*)block_addr;
    block->size = size;
    block->is_free = 0;
    block->next = 0;

    if(free_list == 0){
        free_list = block;
    }else{
        memory_block_t* last = free_list;
        while(last->next){
            last = last->next;
        }
        last->next = block;
    }
    heap_current += total_size;
    return (void*)(block_addr + MEMORY_BLOCK_HEADER_SIZE);
}

void kfree(void* ptr){
    if(!ptr) return;

    memory_block_t* block = (memory_block_t*)((uint64_t)ptr - MEMORY_BLOCK_HEADER_SIZE);
    block->is_free = 1;

    memory_block_t* current = free_list;
    while(current && current->next){
        if(current->is_free && current->next->is_free){
            current->size += current->next->size + MEMORY_BLOCK_HEADER_SIZE;
            current->next = current->next->next;
        }else{
            current = current->next;
        }
    }
}

void* kcalloc(size_t num, size_t size){
    size_t total_size = num * size;

    void* ptr = kmalloc(total_size);

    if(ptr){
        memset(ptr, 0, total_size);
    }

    return ptr;
}

void* krealloc(void* ptr, size_t new_size){
    if(!ptr) return kmalloc(new_size);

    if(new_size == 0){
        kfree(ptr);
    }

    memory_block_t* block = (memory_block_t*)((uint64_t)ptr - MEMORY_BLOCK_HEADER_SIZE);

    if(block->size >= new_size){
        return ptr;
    }

    void* new_ptr = kmalloc(new_size);
    if(!new_ptr) return 0;

    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)new_ptr;
    for(size_t i = 0; i < block->size; i++){
        dst[i] = src[i];
    }

    kfree(ptr);
    return new_ptr;
}

uint64_t memory_get_free(){
    return heap_end - heap_current;
}

uint64_t memory_get_used(){
    return heap_current - heap_start;
}

uint64_t memory_get_total(){
    return heap_end - heap_start;
}

void memory_print_stats() {
    vga_puts("\n=== Memory Statistics ===\n");

    char num_str[32];
    uint64_t total_size = heap_end - heap_start;
    uint64_t used_size = heap_current - heap_start;
    uint64_t free_size = heap_end - heap_current;

    vga_puts("Total heap: ");
    uitoa(total_size / 1024, num_str);
    vga_puts(num_str);
    vga_puts(" KB\n");

    vga_puts("Used: ");
    uitoa(used_size, num_str);
    vga_puts(num_str);
    vga_puts(" bytes\n");

    vga_puts("Free: ");
    uitoa(free_size / 1024, num_str);
    vga_puts(num_str);
    vga_puts(" KB\n");

    // Count blocks
    uint64_t total_blocks = 0;
    uint64_t free_blocks = 0;
    memory_block_t* current = free_list;

    while (current) {
        total_blocks++;
        if (current->is_free) {
            free_blocks++;
        }
        current = current->next;
    }

    vga_puts("Total blocks: ");
    uitoa(total_blocks, num_str);
    vga_puts(num_str);
    vga_puts("\n");

    vga_puts("Free blocks: ");
    uitoa(free_blocks, num_str);
    vga_puts(num_str);
    vga_puts("\n\n");
}
