#ifndef SLAB_H
#define SLAB_H

#include "libc/stdint.h"
#include "libc/stddef.h"
#include "error_handling/errno.h"

/*
 * Slab Allocator
 * ==============
 * Efficient allocator for fixed-size objects with object caching.
 * 
 * Features:
 * - Fast allocation (O(1) for cached objects)
 * - Reduces fragmentation for common object sizes
 * - Object reuse reduces initialization overhead
 * - Cache-line alignment for performance
 * 
 * Design:
 * - Each cache manages objects of one size
 * - Objects are grouped into slabs (one or more pages)
 * - Slabs can be: full, partial, or empty
 * - Free objects are tracked via linked list
 */

#define SLAB_NAME_MAX 32
#define SLAB_MAX_CACHES 32

// Slab states
typedef enum {
    SLAB_EMPTY,    // All objects free
    SLAB_PARTIAL,  // Some objects free
    SLAB_FULL      // No objects free
} slab_state_t;

// Forward declarations
struct slab;
struct slab_cache;

// Object header (stores free list pointer)
typedef struct slab_object {
    struct slab_object* next;
} slab_object_t;

// Slab structure (represents one or more pages of objects)
typedef struct slab {
    struct slab* next;          // Next slab in list
    struct slab* prev;          // Previous slab in list
    struct slab_cache* cache;   // Parent cache
    
    void* objects;              // Start of object area
    slab_object_t* free_list;   // Free objects in this slab
    
    uint32_t num_objects;       // Total objects in slab
    uint32_t free_objects;      // Number of free objects
    
    slab_state_t state;
} slab_t;

// Slab cache (manages slabs for one object size)
typedef struct slab_cache {
    char name[SLAB_NAME_MAX];   // Cache identifier
    
    size_t object_size;         // Size of each object
    size_t aligned_size;        // Size with alignment
    uint32_t objects_per_slab;  // Objects per slab
    uint32_t slab_order;        // Pages per slab (2^order)
    
    // Slab lists
    slab_t* slabs_full;         // Fully allocated slabs
    slab_t* slabs_partial;      // Partially allocated slabs
    slab_t* slabs_empty;        // Empty slabs (can be freed)
    
    // Statistics
    uint64_t num_allocations;
    uint64_t num_frees;
    uint64_t num_slabs;
    uint64_t num_active_objects;
    
    // Constructor/destructor (optional)
    void (*ctor)(void* obj);    // Called on first allocation
    void (*dtor)(void* obj);    // Called before freeing
} slab_cache_t;

// Initialize slab allocator subsystem
kerr_t slab_init(void);

// Create a new slab cache
slab_cache_t* slab_cache_create(const char* name, size_t object_size,
                                 void (*ctor)(void*), void (*dtor)(void*));

// Destroy a slab cache (frees all slabs)
void slab_cache_destroy(slab_cache_t* cache);

// Allocate object from cache
void* slab_alloc(slab_cache_t* cache);

// Free object back to cache
void slab_free(slab_cache_t* cache, void* obj);

// Shrink cache by freeing empty slabs
uint32_t slab_cache_shrink(slab_cache_t* cache);

// Print cache statistics
void slab_cache_print_stats(slab_cache_t* cache);

// Print all cache statistics
void slab_print_all_stats(void);

// Common caches (created at init)
extern slab_cache_t* kmalloc_cache_32;
extern slab_cache_t* kmalloc_cache_64;
extern slab_cache_t* kmalloc_cache_128;
extern slab_cache_t* kmalloc_cache_256;
extern slab_cache_t* kmalloc_cache_512;
extern slab_cache_t* kmalloc_cache_1024;
extern slab_cache_t* kmalloc_cache_2048;
extern slab_cache_t* kmalloc_cache_4096;

// Utility: Allocate from appropriate slab cache based on size
void* slab_kmalloc(size_t size);

// Utility: Free to slab cache (finds correct cache)
void slab_kfree(void* obj);

#endif