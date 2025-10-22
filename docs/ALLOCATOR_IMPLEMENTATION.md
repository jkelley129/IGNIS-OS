# IGNIS OS - Advanced Memory Allocator Implementation

## Overview

This implementation adds two sophisticated memory allocators to IGNIS OS:

1. **Buddy Allocator** - Efficient page-level allocation with power-of-2 sizes
2. **Slab Allocator** - Object caching system for fixed-size allocations

## Architecture

```
┌─────────────────────────────────────────────────────┐
│              Application Layer                       │
│          (kmalloc, kfree, etc.)                     │
└─────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────┐
│           Slab Allocator (slab.c)                   │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐          │
│  │ Cache 32 │ │ Cache 64 │ │Cache 128 │ ...      │
│  └──────────┘ └──────────┘ └──────────┘          │
└─────────────────────────────────────────────────────┘
                    │
                    ▼ (for large allocations)
┌─────────────────────────────────────────────────────┐
│          Buddy Allocator (buddy.c)                  │
│  Orders: 0(4KB) 1(8KB) 2(16KB) ... 11(8MB)        │
└─────────────────────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────┐
│        Physical Memory Manager (PMM)                 │
│              Raw Page Frames                         │
└─────────────────────────────────────────────────────┘
```

## Buddy Allocator

### Features

- **Fast Allocation**: O(log n) time complexity
- **Low Fragmentation**: Automatically coalesces adjacent free blocks
- **Power-of-2 Sizes**: Allocates in blocks of 2^n pages (4KB to 8MB)
- **Efficient Memory Use**: Minimizes wasted space through merging

### How It Works

The buddy allocator maintains free lists for each order (power of 2):

```
Order 0: 4KB blocks   (1 page)
Order 1: 8KB blocks   (2 pages)
Order 2: 16KB blocks  (4 pages)
...
Order 11: 8MB blocks  (2048 pages)
```

**Allocation Process:**
1. Find smallest order that fits requested size
2. If no block available, split larger block
3. Mark block as allocated in bitmap
4. Return physical address

**Deallocation Process:**
1. Mark block as free
2. Calculate buddy address (XOR relationship)
3. If buddy is also free, merge them
4. Recursively try to merge at higher orders

### API Usage

```c
#include "mm/buddy.h"

// Initialize buddy allocator
buddy_allocator_t allocator;
kerr_t err = buddy_init(&allocator, PHYS_BASE, SIZE);

// Allocate 16KB (will round up to nearest power of 2)
uint64_t phys_addr = buddy_alloc(&allocator, 16384);

// Allocate specific order (order 2 = 16KB)
uint64_t phys_addr2 = buddy_alloc_order(&allocator, 2);

// Free memory
buddy_free(&allocator, phys_addr);

// Get statistics
uint64_t free_mem = buddy_get_free_memory(&allocator);
buddy_print_stats(&allocator);
```

### Integration Example

```c
// In kernel initialization:
kerr_t memory_init_advanced(void) {
    // Initialize buddy allocator for kernel heap region
    buddy_allocator_t* buddy = buddy_get_global();
    
    // Reserve 64MB for buddy allocator
    uint64_t buddy_base = PHYS_FREE_START;
    uint64_t buddy_size = 64 * 1024 * 1024; // 64MB
    
    kerr_t err = buddy_init(buddy, buddy_base, buddy_size);
    if (err != E_OK) {
        return err;
    }
    
    console_puts("Buddy allocator initialized\n");
    return E_OK;
}
```

## Slab Allocator

### Features

- **O(1) Allocation**: Constant time for cached objects
- **Reduced Fragmentation**: Groups same-size objects
- **Object Reuse**: Caches reduce initialization overhead
- **Cache-line Alignment**: Better CPU cache performance

### How It Works

The slab allocator groups objects into slabs:

```
Cache (e.g., 64-byte objects)
│
├─ Slab 1 (4KB page)
│  ├─ Object 1 [free]
│  ├─ Object 2 [allocated]
│  ├─ Object 3 [free]
│  └─ ...
│
├─ Slab 2 (4KB page)
│  └─ All objects [free]
│
└─ Slab 3 (4KB page)
   └─ All objects [allocated]
```

**Slab States:**
- **Empty**: All objects free (can be released)
- **Partial**: Some objects free (preferred for allocation)
- **Full**: No objects free

### API Usage

```c
#include "mm/slab.h"

// Initialize slab subsystem
slab_init();

// Create custom cache
slab_cache_t* my_cache = slab_cache_create(
    "my_objects",           // Name
    sizeof(my_struct_t),    // Object size
    my_constructor,         // Constructor (optional)
    my_destructor          // Destructor (optional)
);

// Allocate object
my_struct_t* obj = slab_alloc(my_cache);

// Free object
slab_free(my_cache, obj);

// Or use convenience functions for common sizes
void* ptr = slab_kmalloc(128);  // Uses kmalloc-128 cache
slab_kfree(ptr);

// Shrink cache (free empty slabs)
uint32_t freed = slab_cache_shrink(my_cache);

// Print statistics
slab_cache_print_stats(my_cache);
slab_print_all_stats();
```

### Pre-created Caches

The slab allocator creates 8 common-size caches at initialization:

```c
kmalloc_cache_32    // 32-byte allocations
kmalloc_cache_64    // 64-byte allocations
kmalloc_cache_128   // 128-byte allocations
kmalloc_cache_256   // 256-byte allocations
kmalloc_cache_512   // 512-byte allocations
kmalloc_cache_1024  // 1KB allocations
kmalloc_cache_2048  // 2KB allocations
kmalloc_cache_4096  // 4KB allocations
```

## Integration into IGNIS

### Step 1: Update Makefile

Add new object files to `Makefile`:

```makefile
OBJS = ... \
       $(BUILD_DIR)/buddy.o \
       $(BUILD_DIR)/slab.o

$(BUILD_DIR)/buddy.o: mm/buddy.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/slab.o: mm/slab.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@
```

### Step 2: Initialize in kernel.c

Update `kernel_main()` to initialize new allocators:

```c
#include "mm/buddy.h"
#include "mm/slab.h"

void kernel_main() {
    // ... existing initialization ...
    
    // Initialize buddy allocator
    buddy_allocator_t* buddy = kalloc_pages(1); // Allocate structure
    uint64_t buddy_base = PHYS_FREE_START + 0x4000000; // Start at 64MB
    uint64_t buddy_size = 64 * 1024 * 1024;            // 64MB region
    
    TRY_INIT("Buddy Allocator", buddy_init(buddy, buddy_base, buddy_size), err_count);
    
    // Initialize slab allocator
    TRY_INIT("Slab Allocator", slab_init(), err_count);
    
    // ... rest of initialization ...
}
```

### Step 3: Add Shell Commands

Add diagnostic commands to shell:

```c
// In shell.h
void cmd_buddyinfo(int argc, char** argv);
void cmd_buddytest(int argc, char** argv);
void cmd_slabinfo(int argc, char** argv);
void cmd_slabtest(int argc, char** argv);

// In shell.c
static const shell_command_t commands[] = {
    // ... existing commands ...
    {"buddyinfo", "Show buddy allocator stats", cmd_buddyinfo},
    {"buddytest", "Test buddy allocator", cmd_buddytest},
    {"slabinfo", "Show slab allocator stats", cmd_slabinfo},
    {"slabtest", "Test slab allocator", cmd_slabtest},
    // ...
};

void cmd_buddyinfo(int argc, char** argv) {
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        buddy_print_stats(buddy);
    } else {
        console_perror("Buddy allocator not initialized\n");
    }
}

void cmd_buddytest(int argc, char** argv) {
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) {
        console_perror("Buddy allocator not initialized\n");
        return;
    }
    
    console_puts("\n=== Buddy Allocator Test ===\n");
    
    // Test various sizes
    console_puts("Allocating 4KB... ");
    uint64_t ptr1 = buddy_alloc(buddy, 4096);
    console_puts(ptr1 ? "OK\n" : "FAILED\n");
    
    console_puts("Allocating 16KB... ");
    uint64_t ptr2 = buddy_alloc(buddy, 16384);
    console_puts(ptr2 ? "OK\n" : "FAILED\n");
    
    console_puts("Allocating 1MB... ");
    uint64_t ptr3 = buddy_alloc(buddy, 1048576);
    console_puts(ptr3 ? "OK\n" : "FAILED\n");
    
    // Free and test merging
    console_puts("Freeing 16KB... ");
    buddy_free(buddy, ptr2);
    console_puts("OK\n");
    
    console_puts("Freeing 4KB... ");
    buddy_free(buddy, ptr1);
    console_puts("OK (should merge)\n");
    
    console_puts("Freeing 1MB... ");
    buddy_free(buddy, ptr3);
    console_puts("OK\n\n");
    
    buddy_print_stats(buddy);
}

void cmd_slabinfo(int argc, char** argv) {
    slab_print_all_stats();
}

void cmd_slabtest(int argc, char** argv) {
    console_puts("\n=== Slab Allocator Test ===\n");
    
    // Test small allocations
    console_puts("Allocating 10x 64-byte objects... ");
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = slab_kmalloc(64);
    }
    console_puts("OK\n");
    
    // Free half
    console_puts("Freeing 5 objects... ");
    for (int i = 0; i < 5; i++) {
        slab_kfree(ptrs[i]);
    }
    console_puts("OK\n");
    
    // Allocate again (should reuse)
    console_puts("Re-allocating 5 objects (should reuse)... ");
    for (int i = 0; i < 5; i++) {
        ptrs[i] = slab_kmalloc(64);
    }
    console_puts("OK\n");
    
    // Clean up
    console_puts("Freeing all objects... ");
    for (int i = 0; i < 10; i++) {
        slab_kfree(ptrs[i]);
    }
    console_puts("OK\n\n");
    
    slab_print_all_stats();
}
```

## Performance Characteristics

### Buddy Allocator

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Allocation | O(log n) | May need to split blocks |
| Deallocation | O(log n) | May merge with buddy |
| Best for | Page-sized allocations | 4KB to 8MB |

### Slab Allocator

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| Allocation | O(1) | From free list |
| Deallocation | O(n) | Must find owning slab |
| Best for | Small, frequent allocations | < 4KB |

## Memory Layout After Integration

```
Physical Memory:
0x00000000 - 0x00100000 : Low memory (BIOS, VGA)
0x00100000 - 0x00200000 : Kernel code/data
0x00200000 - 0x00300000 : Initial heap (old kmalloc)
0x00300000 - 0x00400000 : PMM bitmap
0x00400000 - 0x04000000 : PMM managed pages (60MB)
0x04000000 - 0x08000000 : Buddy allocator region (64MB)
                          ├─ Buddy bitmap
                          ├─ Slab caches
                          └─ Dynamic allocations
```

## Usage Recommendations

### When to Use Each Allocator

**Buddy Allocator:**
- Page-aligned allocations
- Large blocks (> 4KB)
- DMA buffers
- Page tables
- File system buffers

**Slab Allocator:**
- Frequently allocated objects
- Fixed-size structures
- Cache-line aligned data
- < 4KB allocations

**Original kmalloc:**
- Temporary/small allocations
- Variable sizes
- Simple use cases

### Example: Allocating a Buffer for Block I/O

```c
// Bad: Uses slow path
void* buffer = kmalloc(4096);

// Better: Uses buddy allocator directly
uint64_t phys = buddy_alloc(buddy_get_global(), 4096);
void* buffer = PHYS_TO_VIRT(phys);

// Best: Use helper
void* buffer = kalloc_pages(1); // Allocates 1 page
```

### Example: Managing Driver Structures

```c
// Create cache for driver-specific structure
typedef struct {
    uint32_t id;
    uint8_t status;
    void* data;
} my_driver_obj_t;

slab_cache_t* driver_cache = slab_cache_create(
    "driver_objects",
    sizeof(my_driver_obj_t),
    NULL, NULL
);

// Fast allocation
my_driver_obj_t* obj = slab_alloc(driver_cache);

// Use object...

// Fast deallocation
slab_free(driver_cache, obj);
```

## Testing

### Unit Tests

Run the test commands:

```
ignis$ buddytest
ignis$ slabtest
ignis$ memtest
```

### Stress Test

```c
void memory_stress_test() {
    console_puts("=== Memory Stress Test ===\n");
    
    // Allocate many objects of various sizes
    void* ptrs[100];
    
    for (int i = 0; i < 100; i++) {
        size_t size = (i % 8 + 1) * 64; // 64 to 512 bytes
        ptrs[i] = slab_kmalloc(size);
    }
    
    // Free in random order
    for (int i = 0; i < 100; i += 2) {
        slab_kfree(ptrs[i]);
    }
    
    // Allocate again
    for (int i = 0; i < 100; i += 2) {
        size_t size = (i % 8 + 1) * 64;
        ptrs[i] = slab_kmalloc(size);
    }
    
    // Clean up
    for (int i = 0; i < 100; i++) {
        slab_kfree(ptrs[i]);
    }
    
    console_puts("Stress test complete!\n");
}
```

## Future Enhancements

1. **NUMA Awareness**: Per-node allocators
2. **Statistics**: Detailed profiling of allocations
3. **Debug Mode**: Guard pages, overflow detection
4. **Compaction**: Defragmentation for long-running systems
5. **Per-CPU Caches**: Lock-free allocation paths

## References

- "The Slab Allocator: An Object-Caching Kernel Memory Allocator" - Jeff Bonwick
- "Buddy Memory Allocation" - Donald Knuth, TAOCP Vol 1
- Linux Kernel SLUB allocator implementation
- FreeBSD UMA (Universal Memory Allocator)

---

**Author**: Josh Kelley  
**Date**: January 2025  
**IGNIS OS Version**: 0.0.01