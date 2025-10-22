# IGNIS OS Memory Layout

## Overview

IGNIS uses a higher-half kernel design with a direct physical memory mapping for efficient access to all physical memory. The memory management system consists of three layers: Physical Memory Manager (PMM) for raw page frames, Buddy Allocator for page-level allocations, and Slab Allocator for small object caching.

## Physical Memory Map
```
Physical Address        Size      Description
================================================================================
0x0000000000000000    1 MB      Low Memory (Reserved)
├── 0x0000000000000000    640 KB    Conventional memory
├── 0x00000000000A0000     64 KB    VGA framebuffer
├── 0x00000000000B0000     32 KB    VGA text mode buffer
├── 0x00000000000C0000    128 KB    ROM area
└── 0x00000000000F0000     64 KB    BIOS ROM

0x0000000000100000    1 MB      Kernel Code/Data
├── boot.o sections            Boot code (32-bit)
├── kernel_entry.o             Kernel entry point (64-bit)
├── kernel.o                   Main kernel
├── drivers/*.o                Driver objects
└── other objects              Misc kernel code

0x0000000000200000    1 MB      Initial Heap (Legacy)
└── Simple heap allocator      Used during early boot

0x0000000000300000    1 MB      PMM Bitmap
└── Page frame bitmap          1 bit per 4KB page

0x0000000000400000   60 MB      PMM Free Pages
└── Managed by PMM             Basic page allocation

0x0000000004000000   64 MB      Buddy Allocator Region
├── Buddy bitmap (~2 MB)       Allocation and order tracking
├── Slab caches                Object caches (32B-4KB)
└── Dynamic allocations        Large kernel allocations

0x0000000008000000    ...       Extended Memory (if available)
└── Additional RAM             Not currently used
```

## Virtual Memory Map

### Complete Address Space Layout
```
Virtual Address           Size        Description
================================================================================
USER SPACE (Lower Half - Reserved for Future)
────────────────────────────────────────────────────────────────────────────
0x0000000000000000    128 TB      User Space (Future)
└── 0x0000000000000000            User processes, libraries, stack
    to
    0x00007FFFFFFFFFFF

KERNEL SPACE (Upper Half - Current Implementation)
────────────────────────────────────────────────────────────────────────────
0xFFFF800000000000    512 GB      Physical Memory Direct Map
├── Maps all physical RAM 1:1      Access any physical address efficiently
├── Offset: PHYS + 0xFFFF800000000000
└── Used by: PMM, VMM, Buddy, Slab, DMA access

0xFFFF808000000000    ...         [Gap - Unmapped]

0xFFFFFFFF80000000    512 MB      Kernel Code/Data
├── 0xFFFFFFFF80000000            .text section (kernel code)
├── 0xFFFFFFFF80100000            .rodata section (read-only data)
├── 0xFFFFFFFF80200000            .data section (initialized data)
└── 0xFFFFFFFF80300000            .bss section (uninitialized data)

0xFFFFFFFFA0000000    512 MB      Kernel Heap (Future Expansion)
└── Extended heap region           For large allocations

0xFFFFFFFFC0000000    512 MB      Kernel Stacks (Future)
├── IRQ stacks                     Per-interrupt stacks
├── Exception stacks               For exception handling
└── Task stacks                    Per-task kernel stacks

0xFFFFFFFFE0000000    512 MB      Reserved
└── Future use                     Device memory, etc.
```

## Memory Management Architecture

### Three-Tier Allocation System
```
┌─────────────────────────────────────────────────────┐
│         Application Layer (kmalloc, kfree)          │
│         Unified kernel memory interface             │
└─────────────────────────────────────────────────────┘
                        │
        ┌───────────────┴───────────────┐
        ▼                               ▼
┌──────────────────┐          ┌──────────────────┐
│ Slab Allocator   │          │ Buddy Allocator  │
│ (32B - 4KB)      │          │ (4KB - 8MB)      │
│ O(1) allocation  │          │ O(log n) alloc   │
└──────────────────┘          └──────────────────┘
        │                               │
        └───────────────┬───────────────┘
                        ▼
        ┌───────────────────────────────┐
        │    Physical Memory Manager    │
        │    (PMM - Raw page frames)    │
        └───────────────────────────────┘
```

### Allocation Routing
```
Request Size          Allocator Used       Time Complexity
═══════════════════════════════════════════════════════════
≤ 32 bytes            Slab (32B cache)     O(1)
≤ 64 bytes            Slab (64B cache)     O(1)
≤ 128 bytes           Slab (128B cache)    O(1)
≤ 256 bytes           Slab (256B cache)    O(1)
≤ 512 bytes           Slab (512B cache)    O(1)
≤ 1 KB                Slab (1KB cache)     O(1)
≤ 2 KB                Slab (2KB cache)     O(1)
≤ 4 KB                Slab (4KB cache)     O(1)
> 4 KB                Buddy allocator      O(log n)
Page-aligned          Buddy allocator      O(log n)
```

## Detailed Memory Regions

### 1. Low Memory (0x00000000 - 0x00100000)

#### VGA Memory (0xB8000)
```
Physical: 0x00000000000B8000
Virtual:  0xFFFF8000000B8000 (via direct map)
Size:     32 KB (80x25 text mode = 4000 bytes, rest unused)
Format:   2 bytes per character (char + attribute)
```

### 2. Kernel Region (0x00100000 - 0x00200000)

#### Section Layout
```
Physical Address    Virtual Address         Section    Size
────────────────────────────────────────────────────────────
0x00100000         0xFFFFFFFF80000000      .text      ~400 KB
0x00164000         0xFFFFFFFF80064000      .rodata    ~100 KB
0x00182000         0xFFFFFFFF80082000      .data      ~50 KB
0x00190000         0xFFFFFFFF80090000      .bss       ~450 KB
```

### 3. Initial Heap (0x00200000 - 0x00300000)
```
Legacy heap allocator used during early boot before
buddy and slab allocators are initialized.

Structure:
┌─────────────────────────────────────────┐
│  heap_t structure (meta-data)           │ 0x00200000
├─────────────────────────────────────────┤
│  Free list blocks (rarely used)         │
│  Most allocations now go to buddy/slab  │
└─────────────────────────────────────────┘ 0x00300000
```

### 4. PMM Bitmap (0x00300000 - 0x00400000)
```
Bitmap Layout:
┌────────────────────────────────────────┐
│ 1 bit = 1 page (4 KB)                  │
│ 1 byte = 8 pages (32 KB)               │
│ 1 KB bitmap = 8192 pages (32 MB)       │
│                                         │
│ Total size: 1 MB                        │
│ Manages: 32,768 pages (128 MB)         │
└────────────────────────────────────────┘

Bit meaning:
  0 = Page is free
  1 = Page is allocated
```

### 5. PMM Free Pages (0x00400000 - 0x04000000)
```
Basic page allocator (60 MB):
┌─────────────────────────────────────────┐
│ Page 0    │ Free  │ 0x00400000          │
│ Page 1    │ Free  │ 0x00401000          │
│ Page 2    │ Alloc │ 0x00402000          │
│ ...                                      │
│ Used for buddy allocator structures     │
│ and slab cache pages                    │
└─────────────────────────────────────────┘
```

### 6. Buddy Allocator Region (0x04000000 - 0x08000000)

#### Memory Organization (64 MB)
```
Physical: 0x04000000 - 0x08000000
Virtual:  0xFFFF800004000000 - 0xFFFF800008000000

Layout:
┌─────────────────────────────────────────────────────┐
│ Allocation Bitmap (~1 MB)                           │
│   - 1 bit per page (16,384 pages)                   │
│   - Tracks allocated vs free pages                  │
├─────────────────────────────────────────────────────┤
│ Order Bitmap (~1 MB)                                │
│   - 1 byte per page                                 │
│   - Stores allocation order (0-11)                  │
├─────────────────────────────────────────────────────┤
│ Free Block Regions (organized by order)             │
│                                                      │
│ Order 0 (4 KB):   ████░░░░░░░░░░░░                 │
│ Order 1 (8 KB):   ██░░░░░░░░                       │
│ Order 2 (16 KB):  █░░░░░░                          │
│ Order 3 (32 KB):  █░░░░                            │
│ ...                                                  │
│ Order 11 (8 MB):  █                                 │
│                                                      │
│ Slab Cache Pages:                                   │
│   - kmalloc-32, 64, 128, 256, 512, 1024, 2048, 4096│
│   - Custom caches as needed                         │
└─────────────────────────────────────────────────────┘

Legend: █ = used, ░ = free
```

#### Buddy Free Lists
```
Order  Block Size   Max Blocks   Use Case
═══════════════════════════════════════════════════════
0      4 KB         16,384       Small allocations
1      8 KB         8,192        Typical slab slabs
2      16 KB        4,096        Medium allocations
3      32 KB        2,048        Large buffers
4      64 KB        1,024        Very large buffers
5      128 KB       512          DMA buffers
6      256 KB       256          Large data structures
7      512 KB       128          Massive buffers
8      1 MB         64           File caches
9      2 MB         32           Huge pages
10     4 MB         16           Very large allocations
11     8 MB         8            Maximum allocation
```

#### Slab Allocator Caches
```
Cache Name      Object Size   Slab Size   Objects/Slab   Status
═════════════════════════════════════════════════════════════════
kmalloc-32      32 bytes      4 KB        128            Active
kmalloc-64      64 bytes      4 KB        64             Active
kmalloc-128     128 bytes     8 KB        64             Active
kmalloc-256     256 bytes     16 KB       64             Active
kmalloc-512     512 bytes     16 KB       32             Active
kmalloc-1024    1 KB          16 KB       16             Active
kmalloc-2048    2 KB          16 KB       8              Active
kmalloc-4096    4 KB          4 KB        1              Active

Slab States:
  Empty   - All objects free (can be released)
  Partial - Some objects free (preferred for allocation)
  Full    - No objects free (no allocations possible)
```

## Memory Allocation Examples

### Example 1: Small Allocation (64 bytes)
```
Request: ptr = kmalloc(64)

Flow:
1. kmalloc() routes to slab_kmalloc(64)
2. Finds kmalloc-64 cache
3. Checks partial slabs → Found slab with free objects
4. Pops object from free list
5. Returns pointer in O(1) time

Memory Layout:
┌─────────────────────────────────────────┐
│ Slab Header (slab_t)                    │
├─────────────────────────────────────────┤
│ Object 0 [ALLOCATED] ← returned pointer │
│ Object 1 [FREE]                         │
│ Object 2 [ALLOCATED]                    │
│ Object 3 [FREE]                         │
│ ...                                      │
│ Object 63 [FREE]                        │
└─────────────────────────────────────────┘
```

### Example 2: Large Allocation (64 KB)
```
Request: ptr = kmalloc(65536)

Flow:
1. kmalloc() detects size > 4KB
2. Routes to buddy allocator
3. Calculates order: 65536 bytes = 16 pages = order 4
4. Checks free_lists[4] → Found free 64KB block
5. Removes block from free list
6. Marks pages in allocation bitmap
7. Stores order in order bitmap
8. Returns virtual address

Buddy Block:
┌─────────────────────────────────────────┐
│ Header (buddy_alloc_header_t)           │
│   - magic: 0xB0DD1E5                    │
│   - order: 4                            │
│   - size: 65536                         │
├─────────────────────────────────────────┤
│ User Data (65536 bytes)                 │ ← returned pointer
│ ...                                      │
└─────────────────────────────────────────┘
```

### Example 3: Page Allocation
```
Request: ptr = kalloc_pages(1)

Flow:
1. kalloc_pages() calls buddy_alloc_order(buddy, 0)
2. Checks free_lists[0] for 4KB blocks
3. If empty, splits order 1 block into two order 0 blocks
4. Returns physical address
5. Converts to virtual via PHYS_TO_VIRT()

Buddy Split Operation:
Before:
  Order 1: [8KB Block A]
  Order 0: []

After split:
  Order 1: []
  Order 0: [4KB Block A1] [4KB Block A2]
           ^ returned     ^ added to free list
```

### Example 4: Buddy Merging
```
Request: kfree(ptr)  // Free 4KB block

Flow:
1. kfree() detects buddy allocation (magic number)
2. Gets allocation order from header
3. buddy_free() calculates buddy address
4. Checks if buddy is also free
5. If yes, merges blocks and recursively tries higher orders

Buddy Merge:
Before:
  Order 0: [4KB Free Block A] [4KB Free Block B]
           ^ buddies (adjacent)

After merge:
  Order 0: []
  Order 1: [8KB Block AB]
           ^ merged block

Buddy Address Calculation:
  buddy_addr = block_addr ^ (order * PAGE_SIZE)
  
  Example:
    Block at 0x04100000, order 0
    Buddy at 0x04101000 = 0x04100000 ^ 0x1000
```

## Page Table Structure

### 4-Level Paging
```
Virtual Address (64-bit):
┌──────────┬─────┬─────┬─────┬─────┬──────────┐
│  Sign    │ PML4│PDPT │ PD  │ PT  │  Offset  │
│  Extend  │     │     │     │     │          │
├──────────┼─────┼─────┼─────┼─────┼──────────┤
│ 63    48 │47 39│38 30│29 21│20 12│ 11     0 │
│  (16)    │ (9) │ (9) │ (9) │ (9) │  (12)    │
└──────────┴─────┴─────┴─────┴─────┴──────────┘

Index calculation:
PML4 index = (addr >> 39) & 0x1FF
PDPT index = (addr >> 30) & 0x1FF
PD index   = (addr >> 21) & 0x1FF
PT index   = (addr >> 12) & 0x1FF
Offset     = addr & 0xFFF
```

### Current Mappings
```
Mapping Type         Virtual Range              Physical Range           Size      Flags
═══════════════════════════════════════════════════════════════════════════════════════
Identity (Boot)      0x0000000000000000        0x0000000000000000      128 MB    P+W+PS
                     - 0x0000000007FFFFFF       - 0x0000000007FFFFFF    (64×2MB)

Higher-Half          0xFFFFFFFF80000000        0x0000000000000000      128 MB    P+W+PS
                     - 0xFFFFFFFF87FFFFFF       - 0x0000000007FFFFFF    (64×2MB)

Direct Map           0xFFFF800000000000        0x0000000000000000      128 MB    P+W+PS
                     - 0xFFFF800007FFFFFF       - 0x0000000007FFFFFF    (64×2MB)

Legend:
  P  = Present
  W  = Writable
  PS = Page Size (2MB pages)
```

## Memory Usage Statistics

### At Boot (After Full Initialization)
```
Region                Physical Range           Size      Status
═══════════════════════════════════════════════════════════════════
Low Memory            0x000000 - 0x100000     1 MB      Reserved
Kernel                0x100000 - 0x200000     1 MB      Used
Legacy Heap           0x200000 - 0x300000     1 MB      Mostly unused
PMM Bitmap            0x300000 - 0x400000     1 MB      Used
PMM Pages             0x400000 - 0x4000000    60 MB     Partially used
Buddy Region          0x4000000 - 0x8000000   64 MB     Managed
  - Bitmaps           0x4000000 - 0x4200000   ~2 MB     Used
  - Free blocks       0x4200000 - 0x8000000   ~62 MB    Available

Total System Memory: 128 MB
Used at Boot: ~6 MB (kernel + bitmaps + structures)
Available: ~122 MB
```

### Runtime Memory Distribution
```
Component              Memory Usage           Allocator
═══════════════════════════════════════════════════════════════
Kernel Code/Data       ~1 MB                  Static
Legacy Heap            ~100 KB                Old allocator
PMM Structures         ~1 MB                  PMM bitmap
Buddy Structures       ~2 MB                  Buddy bitmaps
Slab Caches            Variable               Buddy-backed
  - Cache structures   ~8 × 4 KB = 32 KB     Buddy
  - Initial slabs      ~8 × 4 KB = 32 KB     Buddy
Driver Data            ~50 KB                 Slab/Buddy
VFS/RAMFS              Variable               Slab/Buddy
Block Device Buffers   512B × devices         Slab
Page Tables            ~28 KB                 PMM

Typical per-operation:
  kmalloc(x ≤ 4KB)     x bytes (no overhead)  Slab
  kmalloc(x > 4KB)     round to power-of-2    Buddy + header
  kalloc_pages(n)      n × 4 KB aligned       Buddy
```

## Performance Characteristics

### Allocation Performance
```
Operation               Time Complexity   Notes
═══════════════════════════════════════════════════════════
Slab allocation         O(1)              Pop from free list
Slab deallocation       O(n)              Must find owning slab
Buddy allocation        O(log n)          May need to split
Buddy deallocation      O(log n)          May merge with buddy
Page allocation (PMM)   O(n)              Bitmap scan
Page deallocation       O(1)              Bitmap clear
```

### Memory Overhead
```
Allocator    Per-Allocation Overhead    Cache Overhead
═══════════════════════════════════════════════════════════
Slab         0 bytes                    slab_t per slab
                                        slab_cache_t per cache
Buddy        16 bytes (header)          2 bits per page
                                        (allocation + order)
PMM          0 bytes                    1 bit per page
Legacy       16 bytes (header)          heap_t structure
```

### Fragmentation Characteristics
```
Allocator    Internal Fragmentation    External Fragmentation
═════════════════════════════════════════════════════════════════
Slab         Size - Object Size        Low (same-size objects)
Buddy        Block Size - Request      Controlled by merging
PMM          0 (exact pages)           Can be high
Legacy       Variable                  High (no coalescing)
```

## Debugging Memory Issues

### Common Problems and Solutions
```
Issue: kmalloc returns NULL (small size)
Cause: Slab cache exhausted, buddy can't provide pages
Solution: Check buddy free memory, increase buddy region

Issue: kmalloc returns NULL (large size)
Cause: Buddy allocator fragmented or full
Solution: Run buddy_print_stats(), consider defragmentation

Issue: Page Fault in allocated memory
Cause: Using physical address instead of virtual
Solution: Ensure PHYS_TO_VIRT() conversion

Issue: Double free detected
Cause: Freeing same address twice
Solution: Check buddy_free() warnings in serial.log

Issue: Memory corruption
Cause: Buffer overflow, use-after-free
Solution: Enable guard pages (future), memory debugging
```

### Memory Dump Tools
```
Shell Commands:
  meminfo      - Legacy heap statistics
  pmminfo      - Physical memory stats
  buddyinfo    - Buddy allocator stats
  buddytest    - Test buddy allocation/deallocation
  slabinfo     - Slab cache statistics
  slabtest     - Test slab allocation/deallocation
  pagetest     - Test page allocation

Debug Macros:
  PHYS_TO_VIRT(p)      - Convert physical to virtual
  VIRT_TO_PHYS(v)      - Convert virtual to physical
  IS_PAGE_ALIGNED(a)   - Check 4KB alignment
  BUDDY_MAGIC          - Magic number for buddy allocations
```

### Memory Analysis
```
Analyzing a Memory Leak:

1. Take initial snapshot:
   ignis$ meminfo
   ignis$ buddyinfo
   ignis$ slabinfo

2. Run suspected code

3. Take final snapshot:
   ignis$ meminfo
   ignis$ buddyinfo
   ignis$ slabinfo

4. Compare:
   - Buddy: Check free memory decreased
   - Slab: Check active object count increased
   - Look for caches with high allocation but low free count

5. Check serial.log for:
   - [BUDDY] Warning messages
   - [SLAB] Warning messages
   - Allocation patterns
```

## Memory Allocation Best Practices

### When to Use Each Allocator

```
Size Range          Recommended         Reason
═══════════════════════════════════════════════════════════════
< 32 bytes          Slab (32B)          Fast, no overhead
32-64 bytes         Slab (64B)          Fast, no overhead
64-128 bytes        Slab (128B)         Fast, no overhead
128-256 bytes       Slab (256B)         Fast, no overhead
256-512 bytes       Slab (512B)         Fast, no overhead
512-1024 bytes      Slab (1KB)          Fast, no overhead
1-2 KB              Slab (2KB)          Fast, no overhead
2-4 KB              Slab (4KB)          Fast, no overhead
> 4 KB              Buddy               Only option
Page-aligned        kalloc_pages()      Direct buddy API
Frequent alloc/free Slab                Object reuse
Large buffers       Buddy               Power-of-2 sizing
DMA buffers         kalloc_pages()      Guaranteed alignment
```

### Custom Slab Cache Example
```c
// Create cache for frequently allocated structure
typedef struct packet_buffer {
    uint8_t data[1500];
    uint16_t length;
    uint32_t timestamp;
} packet_buffer_t;

// Create cache
slab_cache_t* packet_cache = slab_cache_create(
    "packets",
    sizeof(packet_buffer_t),
    NULL,  // constructor
    NULL   // destructor
);

// Fast allocation
packet_buffer_t* pkt = slab_alloc(packet_cache);

// Use packet...

// Fast deallocation
slab_free(packet_cache, pkt);
```

## Future Improvements

### Planned Features

1. **NUMA Awareness**
   - Per-node buddy allocators
   - Local memory preferences
   - Cross-node allocation tracking

2. **Advanced Statistics**
   - Per-cache allocation histograms
   - Fragmentation metrics
   - Allocation heat maps

3. **Debug Features**
   - Guard pages (detect overruns)
   - Poisoning (detect use-after-free)
   - Allocation tracking (leak detection)
   - Memory profiling

4. **Optimization**
   - Per-CPU slab caches (lock-free)
   - Lazy coalescing in buddy
   - Slab coloring (cache performance)
   - Memory compaction

5. **New Allocators**
   - SLUB (improved slab)
   - Percpu allocator
   - CMA (Contiguous Memory Allocator)

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*