#ifndef MEMORY_H
#define MEMORY_H

#include "../libc/stdint.h"
#include "../libc/stddef.h"

// Memory region structure
typedef struct {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} __attribute__((packed)) memory_map_entry_t;

// Memory allocation block header
typedef struct memory_block {
    size_t size;
    uint8_t is_free;
    struct memory_block* next;
} memory_block_t;

#define MEMORY_BLOCK_HEADER_SIZE sizeof(memory_block_t)

// Memory allocator functions
void memory_init(uint64_t heap_start, uint64_t heap_size);
void* kmalloc(size_t size);
void kfree(void* ptr);
void* kcalloc(size_t num, size_t size);
void* krealloc(void* ptr, size_t new_size);

// Memory utility functions
void memory_print_stats();
uint64_t memory_get_free();
uint64_t memory_get_used();

#endif