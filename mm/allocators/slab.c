#include "slab.h"
#include "buddy.h"
#include "../memory_layout.h"
#include "../../console/console.h"
#include "../../io/serial.h"
#include "../../libc/string.h"

// Registry of all caches
static slab_cache_t* cache_registry[SLAB_MAX_CACHES];
static uint32_t num_caches = 0;

// Common size caches
slab_cache_t* kmalloc_cache_32 = NULL;
slab_cache_t* kmalloc_cache_64 = NULL;
slab_cache_t* kmalloc_cache_128 = NULL;
slab_cache_t* kmalloc_cache_256 = NULL;
slab_cache_t* kmalloc_cache_512 = NULL;
slab_cache_t* kmalloc_cache_1024 = NULL;
slab_cache_t* kmalloc_cache_2048 = NULL;
slab_cache_t* kmalloc_cache_4096 = NULL;

// Minimum alignment for objects
#define SLAB_ALIGN 8

// Align size to boundary
static inline size_t align_size(size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
}

// Calculate optimal slab order for object size
static uint32_t calculate_slab_order(size_t object_size) {
    // Try to fit at least 8 objects per slab
    size_t min_slab_size = object_size * 8;
    
    // Find smallest order that fits
    for (uint32_t order = 0; order <= 3; order++) {
        if (BUDDY_SIZE_FOR_ORDER(order) >= min_slab_size) {
            return order;
        }
    }
    
    return 2; // Default to 16KB slabs
}

// Remove slab from its current list
static void remove_slab_from_list(slab_t* slab) {
    if (slab->prev) {
        slab->prev->next = slab->next;
    } else {
        // Head of list - update cache pointer
        slab_cache_t* cache = slab->cache;
        switch (slab->state) {
            case SLAB_EMPTY:
                cache->slabs_empty = slab->next;
                break;
            case SLAB_PARTIAL:
                cache->slabs_partial = slab->next;
                break;
            case SLAB_FULL:
                cache->slabs_full = slab->next;
                break;
        }
    }
    
    if (slab->next) {
        slab->next->prev = slab->prev;
    }
    
    slab->next = NULL;
    slab->prev = NULL;
}

// Add slab to appropriate list based on state
static void add_slab_to_list(slab_cache_t* cache, slab_t* slab) {
    slab_t** head;
    
    switch (slab->state) {
        case SLAB_EMPTY:
            head = &cache->slabs_empty;
            break;
        case SLAB_PARTIAL:
            head = &cache->slabs_partial;
            break;
        case SLAB_FULL:
            head = &cache->slabs_full;
            break;
        default:
            return;
    }
    
    slab->next = *head;
    slab->prev = NULL;
    
    if (*head) {
        (*head)->prev = slab;
    }
    
    *head = slab;
}

// Allocate a new slab
static slab_t* allocate_slab(slab_cache_t* cache) {
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return NULL;
    
    // Allocate pages for slab
    uint64_t phys_addr = buddy_alloc_order(buddy, cache->slab_order);
    if (!phys_addr) return NULL;
    
    void* slab_mem = PHYS_TO_VIRT(phys_addr);
    
    // Slab header at start
    slab_t* slab = (slab_t*)slab_mem;
    slab->next = NULL;
    slab->prev = NULL;
    slab->cache = cache;
    slab->num_objects = cache->objects_per_slab;
    slab->free_objects = cache->objects_per_slab;
    slab->state = SLAB_EMPTY;
    
    // Objects start after slab header (aligned)
    size_t header_size = align_size(sizeof(slab_t), SLAB_ALIGN);
    slab->objects = (void*)((uint64_t)slab_mem + header_size);
    
    // Initialize free list
    slab->free_list = NULL;
    uint8_t* obj = (uint8_t*)slab->objects;
    
    for (uint32_t i = 0; i < cache->objects_per_slab; i++) {
        slab_object_t* free_obj = (slab_object_t*)obj;
        free_obj->next = slab->free_list;
        slab->free_list = free_obj;
        
        obj += cache->aligned_size;
    }
    
    cache->num_slabs++;
    
    return slab;
}

// Free a slab
static void free_slab(slab_t* slab) {
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return;
    
    slab_cache_t* cache = slab->cache;
    
    // Call destructor on all objects if provided
    if (cache->dtor) {
        uint8_t* obj = (uint8_t*)slab->objects;
        for (uint32_t i = 0; i < cache->objects_per_slab; i++) {
            cache->dtor(obj);
            obj += cache->aligned_size;
        }
    }
    
    // Free the slab memory
    uint64_t phys_addr = VIRT_TO_PHYS((uint64_t)slab);
    buddy_free(buddy, phys_addr);
    
    cache->num_slabs--;
}

kerr_t slab_init(void) {
    // Initialize cache registry
    for (uint32_t i = 0; i < SLAB_MAX_CACHES; i++) {
        cache_registry[i] = NULL;
    }
    num_caches = 0;
    
    // Create common size caches
    kmalloc_cache_32 = slab_cache_create("kmalloc-32", 32, NULL, NULL);
    kmalloc_cache_64 = slab_cache_create("kmalloc-64", 64, NULL, NULL);
    kmalloc_cache_128 = slab_cache_create("kmalloc-128", 128, NULL, NULL);
    kmalloc_cache_256 = slab_cache_create("kmalloc-256", 256, NULL, NULL);
    kmalloc_cache_512 = slab_cache_create("kmalloc-512", 512, NULL, NULL);
    kmalloc_cache_1024 = slab_cache_create("kmalloc-1024", 1024, NULL, NULL);
    kmalloc_cache_2048 = slab_cache_create("kmalloc-2048", 2048, NULL, NULL);
    kmalloc_cache_4096 = slab_cache_create("kmalloc-4096", 4096, NULL, NULL);
    
    if (!kmalloc_cache_32 || !kmalloc_cache_64 || !kmalloc_cache_128 ||
        !kmalloc_cache_256 || !kmalloc_cache_512 || !kmalloc_cache_1024 ||
        !kmalloc_cache_2048 || !kmalloc_cache_4096) {
        return E_NOMEM;
    }
    
    serial_debug_puts("[SLAB] Initialized with 8 common caches\n");
    
    return E_OK;
}

slab_cache_t* slab_cache_create(const char* name, size_t object_size,
                                 void (*ctor)(void*), void (*dtor)(void*)) {
    if (!name || object_size == 0 || num_caches >= SLAB_MAX_CACHES) {
        return NULL;
    }
    
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return NULL;
    
    // Allocate cache structure from buddy
    uint64_t cache_phys = buddy_alloc(buddy, sizeof(slab_cache_t));
    if (!cache_phys) return NULL;
    
    slab_cache_t* cache = (slab_cache_t*)PHYS_TO_VIRT(cache_phys);
    
    // Initialize cache
    strncpy(cache->name, name, SLAB_NAME_MAX - 1);
    cache->name[SLAB_NAME_MAX - 1] = '\0';
    
    cache->object_size = object_size;
    cache->aligned_size = align_size(object_size, SLAB_ALIGN);
    cache->slab_order = calculate_slab_order(cache->aligned_size);
    
    // Calculate objects per slab
    size_t slab_size = BUDDY_SIZE_FOR_ORDER(cache->slab_order);
    size_t header_size = align_size(sizeof(slab_t), SLAB_ALIGN);
    size_t usable_size = slab_size - header_size;
    cache->objects_per_slab = usable_size / cache->aligned_size;
    
    cache->slabs_full = NULL;
    cache->slabs_partial = NULL;
    cache->slabs_empty = NULL;
    
    cache->num_allocations = 0;
    cache->num_frees = 0;
    cache->num_slabs = 0;
    cache->num_active_objects = 0;
    
    cache->ctor = ctor;
    cache->dtor = dtor;
    
    // Add to registry
    cache_registry[num_caches++] = cache;
    
    return cache;
}

void slab_cache_destroy(slab_cache_t* cache) {
    if (!cache) return;
    
    // Free all slabs
    while (cache->slabs_full) {
        slab_t* slab = cache->slabs_full;
        remove_slab_from_list(slab);
        free_slab(slab);
    }
    
    while (cache->slabs_partial) {
        slab_t* slab = cache->slabs_partial;
        remove_slab_from_list(slab);
        free_slab(slab);
    }
    
    while (cache->slabs_empty) {
        slab_t* slab = cache->slabs_empty;
        remove_slab_from_list(slab);
        free_slab(slab);
    }
    
    // Remove from registry
    for (uint32_t i = 0; i < num_caches; i++) {
        if (cache_registry[i] == cache) {
            // Shift remaining caches
            for (uint32_t j = i; j < num_caches - 1; j++) {
                cache_registry[j] = cache_registry[j + 1];
            }
            cache_registry[num_caches - 1] = NULL;
            num_caches--;
            break;
        }
    }
    
    // Free cache structure
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        uint64_t cache_phys = VIRT_TO_PHYS((uint64_t)cache);
        buddy_free(buddy, cache_phys);
    }
}

void* slab_alloc(slab_cache_t* cache) {
    if (!cache) return NULL;
    
    slab_t* slab = NULL;
    
    // Try partial slabs first
    if (cache->slabs_partial) {
        slab = cache->slabs_partial;
    }
    // Then try empty slabs
    else if (cache->slabs_empty) {
        slab = cache->slabs_empty;
    }
    // Need to allocate new slab
    else {
        slab = allocate_slab(cache);
        if (!slab) return NULL;
        add_slab_to_list(cache, slab);
    }
    
    // Get object from free list
    slab_object_t* obj = slab->free_list;
    if (!obj) return NULL;
    
    slab->free_list = obj->next;
    slab->free_objects--;
    
    // Update slab state
    slab_state_t old_state = slab->state;
    
    if (slab->free_objects == 0) {
        slab->state = SLAB_FULL;
    } else {
        slab->state = SLAB_PARTIAL;
    }
    
    // Move slab if state changed
    if (old_state != slab->state) {
        remove_slab_from_list(slab);
        add_slab_to_list(cache, slab);
    }
    
    // Call constructor on first use
    if (cache->ctor) {
        cache->ctor(obj);
    }
    
    cache->num_allocations++;
    cache->num_active_objects++;
    
    return obj;
}

void slab_free(slab_cache_t* cache, void* obj) {
    if (!cache || !obj) return;
    
    // Find which slab owns this object
    slab_t* slab = NULL;
    
    // Check all slab lists
    slab_t* lists[] = {cache->slabs_full, cache->slabs_partial, cache->slabs_empty};
    
    for (int i = 0; i < 3; i++) {
        slab_t* current = lists[i];
        
        while (current) {
            uint64_t slab_start = (uint64_t)current->objects;
            uint64_t slab_end = slab_start + (cache->objects_per_slab * cache->aligned_size);
            
            if ((uint64_t)obj >= slab_start && (uint64_t)obj < slab_end) {
                slab = current;
                break;
            }
            
            current = current->next;
        }
        
        if (slab) break;
    }
    
    if (!slab) {
        serial_debug_puts("[SLAB] Warning: Object not found in any slab\n");
        return;
    }
    
    // Add to free list
    slab_object_t* free_obj = (slab_object_t*)obj;
    free_obj->next = slab->free_list;
    slab->free_list = free_obj;
    slab->free_objects++;
    
    // Update slab state
    slab_state_t old_state = slab->state;
    
    if (slab->free_objects == slab->num_objects) {
        slab->state = SLAB_EMPTY;
    } else {
        slab->state = SLAB_PARTIAL;
    }
    
    // Move slab if state changed
    if (old_state != slab->state) {
        remove_slab_from_list(slab);
        add_slab_to_list(cache, slab);
    }
    
    cache->num_frees++;
    cache->num_active_objects--;
}

uint32_t slab_cache_shrink(slab_cache_t* cache) {
    if (!cache) return 0;
    
    uint32_t freed = 0;
    
    // Free all empty slabs
    while (cache->slabs_empty) {
        slab_t* slab = cache->slabs_empty;
        remove_slab_from_list(slab);
        free_slab(slab);
        freed++;
    }
    
    return freed;
}

void slab_cache_print_stats(slab_cache_t* cache) {
    if (!cache) return;
    
    console_puts("\nCache: ");
    console_puts(cache->name);
    console_putc('\n');
    
    char num_str[32];
    
    console_puts("  Object size:    ");
    uitoa(cache->object_size, num_str);
    console_puts(num_str);
    console_puts(" bytes\n");
    
    console_puts("  Objects/slab:   ");
    uitoa(cache->objects_per_slab, num_str);
    console_puts(num_str);
    console_putc('\n');
    
    console_puts("  Active objects: ");
    uitoa(cache->num_active_objects, num_str);
    console_puts(num_str);
    console_putc('\n');
    
    console_puts("  Total slabs:    ");
    uitoa(cache->num_slabs, num_str);
    console_puts(num_str);
    console_putc('\n');
    
    console_puts("  Allocations:    ");
    uitoa(cache->num_allocations, num_str);
    console_puts(num_str);
    console_putc('\n');
    
    console_puts("  Frees:          ");
    uitoa(cache->num_frees, num_str);
    console_puts(num_str);
    console_putc('\n');
}

void slab_print_all_stats(void) {
    console_puts("\n=== Slab Allocator Statistics ===\n");
    
    for (uint32_t i = 0; i < num_caches; i++) {
        slab_cache_print_stats(cache_registry[i]);
    }
    
    console_putc('\n');
}

void* slab_kmalloc(size_t size) {
    if (size == 0) return NULL;
    
    // Round up to next power of 2 cache size
    if (size <= 32) return slab_alloc(kmalloc_cache_32);
    if (size <= 64) return slab_alloc(kmalloc_cache_64);
    if (size <= 128) return slab_alloc(kmalloc_cache_128);
    if (size <= 256) return slab_alloc(kmalloc_cache_256);
    if (size <= 512) return slab_alloc(kmalloc_cache_512);
    if (size <= 1024) return slab_alloc(kmalloc_cache_1024);
    if (size <= 2048) return slab_alloc(kmalloc_cache_2048);
    if (size <= 4096) return slab_alloc(kmalloc_cache_4096);
    
    // Fall back to buddy allocator for large allocations
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) return NULL;
    
    uint64_t phys = buddy_alloc(buddy, size);
    if (!phys) return NULL;
    
    return PHYS_TO_VIRT(phys);
}

void slab_kfree(void* obj) {
    if (!obj) return;
    
    // Try each cache
    slab_cache_t* caches[] = {
        kmalloc_cache_32, kmalloc_cache_64, kmalloc_cache_128, kmalloc_cache_256,
        kmalloc_cache_512, kmalloc_cache_1024, kmalloc_cache_2048, kmalloc_cache_4096
    };
    
    for (int i = 0; i < 8; i++) {
        if (!caches[i]) continue;
        
        // Check all slab lists in this cache
        slab_t* lists[] = {
            caches[i]->slabs_full,
            caches[i]->slabs_partial,
            caches[i]->slabs_empty
        };
        
        for (int j = 0; j < 3; j++) {
            slab_t* slab = lists[j];
            
            while (slab) {
                uint64_t slab_start = (uint64_t)slab->objects;
                uint64_t slab_end = slab_start + 
                    (caches[i]->objects_per_slab * caches[i]->aligned_size);
                
                if ((uint64_t)obj >= slab_start && (uint64_t)obj < slab_end) {
                    slab_free(caches[i], obj);
                    return;
                }
                
                slab = slab->next;
            }
        }
    }
    
    // Not found in slab caches - assume it's from buddy allocator
    buddy_allocator_t* buddy = buddy_get_global();
    if (buddy) {
        uint64_t phys = VIRT_TO_PHYS((uint64_t)obj);
        buddy_free(buddy, phys);
    }
}