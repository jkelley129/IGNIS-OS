# IGNIS OS Memory Layout

## Overview

IGNIS uses a higher-half kernel design with a direct physical memory mapping for efficient access to all physical memory. This document describes the complete memory layout in both physical and virtual address spaces.

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

0x0000000000200000    1 MB      Initial Heap
└── kmalloc region             Kernel heap allocator

0x0000000000300000    1 MB      PMM Bitmap
└── Page frame bitmap          1 bit per 4KB page

0x0000000000400000   124 MB     Free Physical Memory
└── Available for allocation   PMM manages this region

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
└── Used by: PMM, VMM, DMA access

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

## Detailed Memory Regions

### 1. Low Memory (0x00000000 - 0x00100000)

#### VGA Memory (0xB8000)
```
Physical: 0x00000000000B8000
Virtual:  0xFFFF8000000B8000 (via direct map)
Size:     32 KB (80x25 text mode = 4000 bytes, rest unused)
Format:   2 bytes per character (char + attribute)
```

#### BIOS Data Area
- **Location**: 0x00000400 - 0x000004FF
- **Purpose**: BIOS configuration and status
- **Access**: Read-only from OS

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

#### Boot Tables (Low Memory)
```
Table Name     Physical Location    Size     Purpose
─────────────────────────────────────────────────────────────
boot_p4        0x00003000          4 KB     PML4 table
boot_p3        0x00004000          4 KB     PDPT (identity)
boot_p2        0x00005000          4 KB     PD (identity)
boot_p3_high   0x00006000          4 KB     PDPT (higher-half)
boot_p2_high   0x00007000          4 KB     PD (higher-half)
boot_p3_direct 0x00008000          4 KB     PDPT (direct map)
boot_p2_direct 0x00009000          4 KB     PD (direct map)
```

### 3. Heap Region (0x00200000 - 0x00300000)
```
Structure:
┌─────────────────────────────────────────┐
│  heap_t structure (meta-data)           │ 0x00200000
├─────────────────────────────────────────┤
│  Free list blocks                       │
│  ┌──────────────────┐                   │
│  │ memory_block_t   │ (allocated)       │
│  ├──────────────────┤                   │
│  │ user data...     │                   │
│  ├──────────────────┤                   │
│  │ memory_block_t   │ (free)            │
│  ├──────────────────┤                   │
│  │ unused space     │                   │
│  └──────────────────┘                   │
│                                          │
│  Grows →                                 │
└─────────────────────────────────────────┘ 0x00300000
```

**Allocation Behavior:**
- First-fit algorithm
- Coalescing of adjacent free blocks
- 8-byte alignment
- Header size: 16 bytes

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

### 5. Free Memory (0x00400000 - 0x08000000)
```
Managed by PMM:
┌─────────────────────────────────────────┐
│ Page 0    │ Free  │ 0x00400000          │
│ Page 1    │ Free  │ 0x00401000          │
│ Page 2    │ Alloc │ 0x00402000          │
│ Page 3    │ Alloc │ 0x00403000          │
│ ...                                      │
│ Page N    │ Free  │ 0x07FFF000          │
└─────────────────────────────────────────┘

Total pages: 30,720 (120 MB)
Page size: 4 KB
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

### Page Table Entry (64-bit)
```
┌────┬────┬────┬────────────────────────────────┬─────────────┐
│ NX │Res │ AVL│    Physical Address[51:12]     │    Flags    │
├────┼────┼────┼────────────────────────────────┼─────────────┤
│ 63 │62:52│51:12│         40 bits               │   11:0      │
└────┴────┴────┴────────────────────────────────┴─────────────┘

Flags (bits 11:0):
  Bit 0  (P)   : Present
  Bit 1  (R/W) : Read/Write
  Bit 2  (U/S) : User/Supervisor
  Bit 3  (PWT) : Page Write-Through
  Bit 4  (PCD) : Page Cache Disable
  Bit 5  (A)   : Accessed
  Bit 6  (D)   : Dirty
  Bit 7  (PAT) : Page Attribute Table
  Bit 8  (G)   : Global
  Bits 9-11    : Available for OS use
  Bit 63 (NX)  : No Execute
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

## Memory Allocation Examples

### Example 1: kmalloc(256)
```
Before:
┌──────────────────┬──────────────────┬──────────────────┐
│ Block: Free      │ Block: Used      │ Block: Free      │
│ Size: 1024       │ Size: 512        │ Size: 2048       │
└──────────────────┴──────────────────┴──────────────────┘
                      heap->current →

After kmalloc(256):
┌──────────────────┬──────────────────┬───────┬──────────┬──────────────────┐
│ Block: Free      │ Block: Used      │ Block │ User     │ Block: Free      │
│ Size: 1024       │ Size: 512        │ 256   │ Data     │ Size: 2048       │
└──────────────────┴──────────────────┴───────┴──────────┴──────────────────┘
                                                 ^ Returned pointer
```

### Example 2: Page Allocation
```
Virtual address request: 0xFFFFFFFFA0000000
Physical address needed: Allocate from PMM

PMM Allocation:
  1. Scan bitmap for free page
  2. Found: Physical 0x00500000
  3. Mark bit as used
  
VMM Mapping:
  1. Get PML4 entry for virt address
  2. Create/get PDPT (if needed)
  3. Create/get PD (if needed)
  4. Create/get PT (if needed)
  5. Set PT entry: 0x00500000 | PAGE_PRESENT | PAGE_WRITE
  6. Flush TLB

Result:
  Virtual 0xFFFFFFFFA0000000 → Physical 0x00500000
```

## Address Translation Examples

### Example 1: Kernel Code Access
```
Source code:          int x = 42;
Compiler generates:   Virtual address: 0xFFFFFFFF80090ABC

Translation:
1. PML4 index = (0xFFFFFFFF80090ABC >> 39) & 0x1FF = 511
2. PDPT index = (0xFFFFFFFF80090ABC >> 30) & 0x1FF = 510
3. PD index   = (0xFFFFFFFF80090ABC >> 21) & 0x1FF = 0
4. PT index   = (0xFFFFFFFF80090ABC >> 12) & 0x1FF = 144
5. Offset     = 0xFFFFFFFF80090ABC & 0xFFF = 0xABC

Hardware TLB lookup:
  → Virtual 0xFFFFFFFF80090ABC
  → Physical 0x00190ABC  (2MB page, direct translation)
  → Access RAM at physical address
```

### Example 2: Direct Map Access
```
Need to access physical address: 0x00500000

Virtual address = PHYS_TO_VIRT(0x00500000)
                = 0x00500000 + 0xFFFF800000000000
                = 0xFFFF800000500000

Access:
  uint8_t* ptr = (uint8_t*)0xFFFF800000500000;
  uint8_t value = *ptr;  // Reads from physical 0x00500000
```

## Memory Usage Statistics

### At Boot (After Initialization)
```
Region                Physical Range           Size      Status
═══════════════════════════════════════════════════════════════════
Low Memory            0x000000 - 0x100000     1 MB      Reserved
Kernel                0x100000 - 0x200000     1 MB      Used
Heap                  0x200000 - 0x300000     1 MB      Partially Used
Bitmap                0x300000 - 0x400000     1 MB      Used
Free Memory           0x400000 - 0x8000000    124 MB    Available

Total System Memory: 128 MB
Used at Boot: ~4 MB
Available: ~124 MB
```

### Typical Runtime Usage
```
Component              Memory Usage
═══════════════════════════════════════
Kernel Code/Data       ~1 MB
Initial Heap           ~100 KB (of 1 MB)
PMM Bitmap             1 MB
Page Tables            ~28 KB (7 tables × 4 KB)
Driver Data            ~50 KB
VFS/RAMFS              Variable (depends on files)
Block Device Buffers   512 bytes × devices

Per-operation:
  kmalloc(x)           x + 16 bytes (header)
  Page allocation      4 KB aligned
  Block I/O            512 bytes (buffer)
```

## Performance Considerations

### TLB (Translation Lookaside Buffer)
```
TLB Characteristics:
  - Hardware cache for page table entries
  - ~64-1024 entries (CPU dependent)
  - Must be flushed after page table changes

Flush methods:
  1. invlpg <addr>     - Flush single page
  2. mov cr3, cr3      - Flush entire TLB
  3. Global bit        - Don't flush on cr3 reload
```

### Cache Performance
```
2MB Pages (Current):
  ✓ Fewer TLB misses
  ✓ Fewer page table levels to traverse
  ✓ Better for kernel code/data
  ✗ More memory waste for small allocations

4KB Pages (Dynamic):
  ✓ Fine-grained control
  ✓ Less memory waste
  ✗ More TLB misses
  ✗ More page table memory
```

## Memory Protection

### Current Protection Levels
```
Region               Readable  Writable  Executable  User
════════════════════════════════════════════════════════════
Kernel .text         Yes       No        Yes         No
Kernel .rodata       Yes       No        No          No
Kernel .data/.bss    Yes       Yes       No          No
Kernel heap          Yes       Yes       No          No
Direct map           Yes       Yes       No          No
User space (future)  Yes       Yes       Yes         Yes
```

### Page Fault Handling
```
Page Fault → INT 14 → page_fault_handler()
                      ↓
                 Analyze error code
                      ↓
         ┌────────────┴────────────┐
         ↓                         ↓
    Not Present              Protection Violation
         ↓                         ↓
    Valid address?           Fix permissions?
    ↓           ↓            ↓           ↓
   Yes          No          Yes          No
    ↓           ↓            ↓           ↓
  Allocate   Kernel       Update      Kernel
  page       Panic        PTE         Panic
```

## Debugging Memory Issues

### Common Problems and Solutions
```
Issue: Page Fault at 0xFFFFFFFFFFFFFFFF
Cause: Uninitialized pointer
Solution: Check initialization, use KASSERT

Issue: Page Fault at 0x0000000000001234
Cause: NULL pointer dereference
Solution: Add NULL checks, use PANIC_ON_NULL

Issue: Triple Fault (CPU reset)
Cause: Page fault in interrupt handler, bad stack
Solution: Check interrupt stack, verify page tables

Issue: kmalloc returns NULL
Cause: Out of heap memory
Solution: Increase heap size, fix memory leaks

Issue: General Protection Fault
Cause: Invalid segment, wrong privilege level
Solution: Check GDT setup, verify mode transitions
```

### Memory Dump Tools
```
Shell Commands:
  meminfo    - Heap statistics
  pmminfo    - Physical memory stats
  pagetest   - Test page allocation

Debug Macros:
  PHYS_TO_VIRT(p)  - Convert physical to virtual
  VIRT_TO_PHYS(v)  - Convert virtual to physical
  IS_PAGE_ALIGNED  - Check alignment
```

## Future Improvements

### Planned Features

1. **User Space**
    - Separate address space per process
    - Copy-on-write (COW) pages
    - Demand paging

2. **Advanced Allocators**
    - Buddy allocator (power-of-2 sizes)
    - Slab allocator (object caching)
    - NUMA awareness

3. **Memory Protection**
    - W^X (Write XOR Execute)
    - ASLR (Address Space Layout Randomization)
    - Stack canaries

4. **Optimization**
    - Huge pages (1GB)
    - Page table compression
    - Memory deduplication

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*