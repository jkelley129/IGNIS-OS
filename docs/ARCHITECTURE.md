# IGNIS OS Architecture

## Overview

IGNIS is a 64-bit x86-64 hobby operating system built from scratch. It features a monolithic kernel design with modular driver architecture, running in higher-half kernel space with sophisticated three-tier memory management (PMM, Buddy, Slab).

## System Architecture Diagram
```
┌─────────────────────────────────────────────────────────────┐
│                        User Space                            │
│                     (Future Development)                     │
└─────────────────────────────────────────────────────────────┘
                            ▲ │
                    System  │ │  Return
                     Calls  │ │  Values
                            │ ▼
┌─────────────────────────────────────────────────────────────┐
│                      Kernel Space                            │
│  ┌────────────────────────────────────────────────────────┐ │
│  │                   Shell Interface                       │ │
│  │            (Command Parser & Executor)                  │ │
│  └────────────────────────────────────────────────────────┘ │
│                            │                                 │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Virtual File System (VFS)                  │ │
│  │        (Unified interface for all filesystems)          │ │
│  └────────────────────────────────────────────────────────┘ │
│           │                    │                             │
│  ┌────────────────┐   ┌───────────────────┐                │
│  │     RAMFS      │   │   Future FS       │                │
│  │  (In-memory)   │   │  (ext4, FAT32)    │                │
│  └────────────────┘   └───────────────────┘                │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Block Device Layer                         │ │
│  │     (Unified I/O abstraction for all storage)          │ │
│  └────────────────────────────────────────────────────────┘ │
│           │                    │                             │
│  ┌────────────────┐   ┌───────────────────┐                │
│  │   ATA Driver   │   │   NVMe Driver     │                │
│  │  (IDE disks)   │   │  (Modern SSDs)    │                │
│  └────────────────┘   └───────────────────┘                │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Memory Management (3-Tier)                 │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐ │ │
│  │  │   PMM    │→ │  Buddy   │→ │   Slab Allocator    │ │ │
│  │  │(Physical)│  │ (Pages)  │  │   (Objects 32B-4KB) │ │ │
│  │  └──────────┘  └──────────┘  └──────────────────────┘ │ │
│  │                      ↓                  ↓               │ │
│  │                 ┌────────────────────────┐             │ │
│  │                 │  kmalloc/kfree (API)   │             │ │
│  │                 └────────────────────────┘             │ │
│  │  ┌──────────┐                                          │ │
│  │  │   VMM    │  Virtual Memory (Page Tables)            │ │
│  │  │(Virtual) │                                           │ │
│  │  └──────────┘                                          │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Driver Framework                           │ │
│  │   (Registration, Priority, Dependency Management)       │ │
│  └────────────────────────────────────────────────────────┘ │
│           │              │              │                    │
│  ┌────────────┐  ┌──────────┐  ┌─────────────┐            │
│  │  Keyboard  │  │   PIT    │  │    Serial   │            │
│  │   Driver   │  │  Timer   │  │   (Debug)   │            │
│  └────────────┘  └──────────┘  └─────────────┘            │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │           Interrupt Handling (IDT)                      │ │
│  │    (Hardware interrupts, Exceptions, Page faults)       │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐ │
│  │              Console Abstraction                        │ │
│  │         (VGA text mode, Serial output)                  │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                      Hardware Layer                          │
│    CPU │ Memory │ Disk Controllers │ Keyboard │ Timer       │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Boot Process
```
BIOS/UEFI → GRUB2 → Multiboot2 Header → boot.asm
                                           ↓
                                    32-bit protected mode
                                           ↓
                                    Enable paging (identity + higher-half)
                                           ↓
                                    64-bit long mode
                                           ↓
                                    kernel_entry.asm
                                           ↓
                                    kernel_main() [Higher-half C code]
```

**Key Features:**
- Multiboot2 compliant bootloader interface
- Identity mapping (0-128MB) for boot code
- Higher-half kernel mapping (0xFFFFFFFF80000000+)
- Direct physical memory map (0xFFFF800000000000+)
- 2MB huge pages for performance

### 2. Memory Management

IGNIS features a sophisticated three-tier memory management system designed for efficiency and flexibility.

#### Physical Memory Manager (PMM)
**Location**: `mm/pmm.c`

- **Bitmap-based allocation**: 1 bit per 4KB page
- **Location**: 3MB-4MB physical memory
- **Tracks**: 128MB of physical RAM (default QEMU config)
- **Manages**: 0x400000 - 0x8000000 (124MB free memory)

**Functions**:
- `pmm_alloc_page()` - Allocate single 4KB page
- `pmm_alloc_pages(n)` - Allocate contiguous pages
- `pmm_free_page()` - Free page
- `pmm_mark_region_used()` - Reserve memory regions

**Performance**: O(n) allocation (bitmap scan), O(1) deallocation

#### Buddy Allocator
**Location**: `mm/allocators/buddy.c`

The buddy allocator manages page-level allocations efficiently using power-of-2 sized blocks.

**Key Features**:
- **Fast Allocation**: O(log n) time complexity
- **Low Fragmentation**: Automatic block coalescing
- **Size Range**: 4KB to 8MB (orders 0-11)
- **Region**: 64MB at 0x04000000 - 0x08000000

**Data Structures**:
```c
typedef struct buddy_allocator {
    uint64_t base_addr;                          // Physical base
    uint64_t total_size;                         // 64MB
    buddy_block_t* free_lists[12];               // One per order
    uint8_t* allocation_bitmap;                  // Track allocated pages
    uint8_t* order_bitmap;                       // Store allocation orders
    uint64_t allocations[12];                    // Statistics
    uint64_t splits;                             // Split count
    uint64_t merges;                             // Merge count
} buddy_allocator_t;
```

**Allocation Process**:
1. Calculate required order (round up to power of 2)
2. Check free list for that order
3. If empty, split larger block recursively
4. Mark pages in allocation bitmap
5. Store order in order bitmap
6. Return physical address

**Deallocation Process**:
1. Get allocation order from order bitmap
2. Mark pages as free
3. Calculate buddy address (XOR relationship)
4. If buddy is also free, merge blocks
5. Recursively try to merge at higher orders

**API**:
```c
// Initialize (called once at boot)
kerr_t buddy_init(buddy_allocator_t* allocator, uint64_t base, uint64_t size);

// Allocate by size (rounds up to power of 2)
uint64_t buddy_alloc(buddy_allocator_t* allocator, size_t size);

// Allocate specific order (0-11)
uint64_t buddy_alloc_order(buddy_allocator_t* allocator, uint8_t order);

// Free allocation
void buddy_free(buddy_allocator_t* allocator, uint64_t phys_addr);

// Statistics
uint64_t buddy_get_free_memory(buddy_allocator_t* allocator);
void buddy_print_stats(buddy_allocator_t* allocator);
```

#### Slab Allocator
**Location**: `mm/allocators/slab.c`

The slab allocator provides O(1) allocation for frequently-used object sizes through caching.

**Key Features**:
- **O(1) Allocation**: Pop from free list
- **Zero Overhead**: No per-allocation headers
- **Object Reuse**: Reduces initialization costs
- **Cache-Line Aligned**: Better CPU cache performance

**Pre-created Caches**:
```
kmalloc-32     (32 bytes)
kmalloc-64     (64 bytes)
kmalloc-128    (128 bytes)
kmalloc-256    (256 bytes)
kmalloc-512    (512 bytes)
kmalloc-1024   (1 KB)
kmalloc-2048   (2 KB)
kmalloc-4096   (4 KB)
```

**Data Structures**:
```c
typedef struct slab_cache {
    char name[32];                    // Cache identifier
    size_t object_size;               // Object size
    size_t aligned_size;              // With alignment
    uint32_t objects_per_slab;        // Objects per slab
    
    slab_t* slabs_full;               // No free objects
    slab_t* slabs_partial;            // Some free objects
    slab_t* slabs_empty;              // All free objects
    
    uint64_t num_allocations;         // Statistics
    uint64_t num_frees;
    uint64_t num_active_objects;
    
    void (*ctor)(void*);              // Constructor
    void (*dtor)(void*);              // Destructor
} slab_cache_t;

typedef struct slab {
    slab_cache_t* cache;              // Parent cache
    void* objects;                    // Object array
    slab_object_t* free_list;         // Free object list
    uint32_t free_objects;            // Free count
    slab_state_t state;               // EMPTY/PARTIAL/FULL
} slab_t;
```

**Allocation Process**:
1. Select appropriate cache for size
2. Check partial slabs first (preferred)
3. If none, check empty slabs
4. If none, allocate new slab from buddy
5. Pop object from free list
6. Call constructor (if defined)
7. Return pointer

**Slab States**:
- **Empty**: All objects free (can be released during shrink)
- **Partial**: Some free objects (preferred for allocation)
- **Full**: No free objects (no allocations possible)

**API**:
```c
// Initialize slab subsystem
kerr_t slab_init(void);

// Create custom cache
slab_cache_t* slab_cache_create(const char* name, size_t object_size,
                                 void (*ctor)(void*), void (*dtor)(void*));

// Allocate from cache
void* slab_alloc(slab_cache_t* cache);

// Free to cache
void slab_free(slab_cache_t* cache, void* obj);

// Convenience functions
void* slab_kmalloc(size_t size);
void slab_kfree(void* obj);
```

#### Unified Kernel Memory API
**Location**: `mm/allocators/kmalloc.c`

The kmalloc layer provides a unified interface that automatically routes allocations to the appropriate allocator.

**Routing Logic**:
```
Size ≤ 32B      → Slab (kmalloc-32)
Size ≤ 64B      → Slab (kmalloc-64)
Size ≤ 128B     → Slab (kmalloc-128)
Size ≤ 256B     → Slab (kmalloc-256)
Size ≤ 512B     → Slab (kmalloc-512)
Size ≤ 1KB      → Slab (kmalloc-1024)
Size ≤ 2KB      → Slab (kmalloc-2048)
Size ≤ 4KB      → Slab (kmalloc-4096)
Size > 4KB      → Buddy allocator
```

**API**:
```c
// Allocate memory
void* kmalloc(size_t size);

// Free memory (automatically detects allocator used)
void kfree(void* ptr);

// Allocate and zero
void* kcalloc(size_t num, size_t size);

// Reallocate
void* krealloc(void* ptr, size_t new_size);

// Page-aligned allocations
void* kmalloc_pages(size_t num_pages);
void kfree_pages(void* ptr, size_t num_pages);

// Statistics
void kmalloc_print_stats(void);
```

**Free Detection**:
The kfree() function uses a magic number to determine which allocator was used:
```c
typedef struct {
    uint32_t magic;     // 0xB0DD1E5 for buddy allocations
    uint32_t order;     // Buddy order
    uint64_t size;      // Original size
} buddy_alloc_header_t;
```

#### Virtual Memory Manager (VMM)
**Location**: `mm/vmm.c`

- **4-level paging**: PML4 → PDPT → PD → PT
- **Page size**: 4KB standard, 2MB huge pages for kernel
- **Handles**: Boot tables in low memory and dynamically allocated tables

**Functions**:
- `vmm_map_page()` - Map virtual to physical
- `vmm_unmap_page()` - Unmap virtual address
- `vmm_get_physical()` - Translate virtual to physical
- `vmm_alloc_page()` - Allocate and map new page

**Key Feature**: Dual table support
- Boot tables (0x3000-0x9000): Identity-mapped, accessible directly
- Dynamic tables (>0x400000): Accessed via PHYS_TO_VIRT()

#### Memory Hierarchy Summary
```
┌──────────────────────────────────────┐
│   Application (kmalloc/kfree)        │
└──────────────────────────────────────┘
         │
         ├─→ Size ≤ 4KB    ──→  ┌──────────────┐
         │                       │ Slab Alloc   │
         │                       │ (O(1))       │
         │                       └──────────────┘
         │                              ↓
         └─→ Size > 4KB    ──→  ┌──────────────┐
                                 │ Buddy Alloc  │
                                 │ (O(log n))   │
                                 └──────────────┘
                                        ↓
                                 ┌──────────────┐
                                 │     PMM      │
                                 │  (O(n)/O(1)) │
                                 └──────────────┘
                                        ↓
                                 ┌──────────────┐
                                 │   Hardware   │
                                 │     RAM      │
                                 └──────────────┘
```

### 3. Driver Framework

The driver framework provides a unified system for managing hardware and software drivers.

#### Driver Structure
```c
typedef struct driver {
    char name[32];              // Driver identifier
    driver_type_t type;         // Type classification
    uint8_t priority;           // Init order (0 = highest)
    driver_status_t status;     // Current state
    
    kerr_t (*init)(struct driver*);
    kerr_t (*cleanup)(struct driver*);
    
    char depends_on[32];        // Dependency name
    void* driver_data;          // Private data
} driver_t;
```

#### Driver Types
- `DRIVER_TYPE_FUNDAMENTAL` - Core system (IDT, Memory)
- `DRIVER_TYPE_BLOCK` - Block storage devices
- `DRIVER_TYPE_INPUT` - Keyboard, mouse
- `DRIVER_TYPE_TIMER` - PIT, APIC timers
- `DRIVER_TYPE_FILESYSTEM` - VFS, RAMFS
- `DRIVER_TYPE_CHAR` - Character devices (future)
- `DRIVER_TYPE_NETWORK` - Network devices (future)

#### Initialization Order
1. **Priority 10**: IDT (interrupts)
2. **Priority 15**: Memory subsystems (PMM, VMM, Buddy, Slab)
3. **Priority 20**: PIT, Keyboard
4. **Priority 30**: Block device layer
5. **Priority 40**: ATA, NVMe drivers
6. **Priority 50**: Filesystems

### 4. Virtual File System (VFS)

Provides a unified interface for all filesystem operations.

#### VFS Node Structure
```c
typedef struct vfs_node {
    char name[MAX_FILENAME];
    file_type_t type;           // FILE_TYPE_REGULAR | FILE_TYPE_DIRECTORY
    size_t size;
    
    struct vfs_node* parent;
    struct filesystem* fs;
    void* fs_data;              // Filesystem-specific data
    
    const vfs_operations_t* ops;
} vfs_node_t;
```

#### Operations
- `vfs_open()` - Open file/directory
- `vfs_read()` - Read file contents
- `vfs_write()` - Write to file
- `vfs_create_file()` - Create new file
- `vfs_create_directory()` - Create directory
- `vfs_delete()` - Remove file/directory
- `vfs_list()` - List directory contents

### 5. Block Device Layer

Abstraction layer for all block storage devices.

#### Block Device Structure
```c
typedef struct block_device {
    uint8_t id;
    block_device_type_t type;   // ATA, AHCI, NVME, RAMDISK
    uint64_t block_count;
    uint16_t block_size;
    uint8_t present;
    char label[32];
    
    void* driver_data;
    const block_device_ops_t* ops;
} block_device_t;
```

#### Supported Devices
- **ATA/IDE**: Primary/Secondary Master/Slave (up to 4 devices)
- **NVMe**: Modern PCIe SSDs (experimental)
- **Future**: AHCI/SATA, USB storage

### 6. Interrupt System

#### IDT (Interrupt Descriptor Table)
- **256 entries**: 64-bit IDT entries
- **Hardware interrupts**: IRQ0-15 (remapped to INT 32-47)
- **Exceptions**: Page faults (INT 14), divide by zero, etc.

#### Interrupt Handlers
```
IRQ0 (INT 32)  → PIT Timer Handler
IRQ1 (INT 33)  → Keyboard Handler
INT 14         → Page Fault Handler
```

#### PIC Configuration
```
Master PIC (IRQ 0-7)  → INT 32-39
Slave PIC (IRQ 8-15)  → INT 40-47
```

### 7. Console System

Abstraction layer for output devices.

#### Console Drivers
- **VGA Text Mode**: 80x25 color text
- **Serial Port**: COM1 debug output

#### Console Operations
```c
typedef struct console_driver {
    kerr_t (*init)(void);
    void (*clear)(void);
    void (*putc)(char c);
    void (*puts)(const char* str);
    void (*set_color)(console_color_attr_t);
    console_color_attr_t (*get_color)(void);
    void (*backspace)(int count);
} console_driver_t;
```

### 8. Shell

Interactive command-line interface.

#### Features
- Command parsing (argc/argv style)
- Built-in commands (30+ commands)
- File system operations
- Memory diagnostics
- Block device testing
- Kernel panic testing

#### Command Categories
- **System**: help, clear, about, uptime, lsdrv
- **Memory**: meminfo, memtest, pmminfo, pagetest, buddyinfo, buddytest, slabinfo, slabtest
- **Filesystem**: ls, tree, touch, mkdir, rm, cat, write, cp
- **Block Devices**: lsblk, blkread, blkwrite, blktest

## Error Handling

### Error Codes (kerr_t)
```c
typedef enum {
    E_OK = 0,           // Success
    E_NOMEM = -1,       // Out of memory
    E_INVALID = -2,     // Invalid argument
    E_NOTFOUND = -3,    // Not found
    E_EXISTS = -4,      // Already exists
    E_NOTDIR = -5,      // Not a directory
    E_ISDIR = -6,       // Is a directory
    E_TIMEOUT = -7,     // Operation timed out
    E_PERM = -8,        // Permission denied
    E_HARDWARE = -9,    // Hardware fault
} kerr_t;
```

### Panic System
- **kernel_panic()** - Fatal error handler
- **PANIC(msg)** - Panic with source location
- **KASSERT(cond, msg)** - Runtime assertion
- **PANIC_ON_NULL(ptr, msg)** - Null pointer check

#### Panic Screen Features
- Blue screen of death style
- System uptime
- Memory statistics
- Register dump (RIP, RSP, RBP, CR2, CR3)
- Source location (file, line, function)
- Serial logging

## Build System

### Makefile Structure
```
all → ignis.iso
     ↓
  compile → boot.o, kernel.o, drivers/*.o, mm/allocators/*.o, etc.
     ↓
  link → kernel.elf (using link.ld)
     ↓
  grub-mkrescue → ignis.iso
```

### Linker Script (link.ld)
- Boot code at physical 1MB (0x100000)
- Kernel at higher-half (0xFFFFFFFF80000000)
- Proper section alignment (4KB pages)

### Compiler Flags
```
-m64                    # 64-bit mode
-ffreestanding          # Freestanding environment
-nostdlib               # No standard library
-fno-pie                # No position independent code
-mcmodel=large          # Large code model
-mno-red-zone           # Disable red zone (kernel requirement)
```

## Testing & Development

### QEMU Testing
```bash
# Basic run
make run

# Run with ATA disk
make run-ata

# Run with NVMe disk
make run-nvme

# Run with all devices
make run-full

# Debug mode
make run-debug
make run-gdb
```

### Serial Debugging
All kernel activity is logged to `serial.log` via COM1 for debugging.

### Memory Testing
- `memtest` - Legacy heap allocator test
- `pagetest` - Page allocation test
- `buddytest` - Buddy allocator test
- `slabtest` - Slab allocator test
- `blktest` - Block device I/O test

## Performance Characteristics

### Memory Allocators

| Allocator | Allocation | Deallocation | Best For |
|-----------|-----------|--------------|----------|
| PMM | O(n) | O(1) | Raw pages |
| Buddy | O(log n) | O(log n) | > 4KB, page-aligned |
| Slab | O(1) | O(n)* | ≤ 4KB, frequent alloc |

*O(n) to find owning slab, but typically fast due to small n

### I/O Performance
- **ATA PIO**: ~3-5 MB/s (polling mode)
- **VGA**: Direct memory access (fast)
- **Serial**: 38400 baud

### Memory Overhead

| Allocator | Per-Allocation | Global |
|-----------|----------------|---------|
| PMM | 0 bytes | 1 bit/page |
| Buddy | 16 bytes (header) | 2 bits/page + 12 free lists |
| Slab | 0 bytes | slab_t per slab + slab_cache_t |

## Future Enhancements

### Short Term
- Per-CPU slab caches (lock-free allocation)
- Memory profiling and leak detection
- Guard pages for overflow detection
- SLUB allocator (improved slab)

### Medium Term
- User space processes
- System calls
- Process scheduler
- Inter-process communication (IPC)

### Long Term
- NUMA-aware memory allocation
- Graphical user interface (GUI)
- Network stack (TCP/IP)
- Audio support
- USB support
- SMP (multi-processor) support

## Code Organization
```
Total Lines of Code: ~10,000
├── Kernel Core:        ~500
├── Memory Management:  ~3,000
│   ├── PMM/VMM:        ~1,000
│   ├── Buddy:          ~500
│   ├── Slab:           ~1,000
│   └── Integration:    ~500
├── Drivers:           ~2,000
├── Filesystem:        ~1,500
├── Shell:             ~2,000
└── Other:             ~1,000
```

## Debugging Tips

### Common Issues
1. **Triple fault**: Usually paging issue, check page table setup
2. **General Protection Fault**: Invalid memory access or descriptor
3. **Page fault**: Invalid virtual address or missing mapping
4. **Freeze**: Likely interrupt issue, check IDT and PIC setup
5. **kmalloc returns NULL**: Check buddy free memory with `buddyinfo`
6. **Slab allocation fails**: Check slab stats with `slabinfo`

### Debug Tools
- Serial output (`serial.log`)
- QEMU monitor (`Ctrl+Alt+2`)
- GDB debugging (`make run-gdb`)
- Kernel panic screen
- Memory commands (`buddyinfo`, `slabinfo`, `pmminfo`)

### Memory Debugging Workflow
```
1. Check overall memory:
   ignis$ meminfo

2. Check physical pages:
   ignis$ pmminfo

3. Check buddy allocator:
   ignis$ buddyinfo

4. Check slab caches:
   ignis$ slabinfo

5. Run tests:
   ignis$ buddytest
   ignis$ slabtest

6. Review serial.log for:
   - [BUDDY] warnings
   - [SLAB] warnings
   - [PMM] errors
```

## References

- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [GRUB Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
- [x86-64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- "The Slab Allocator: An Object-Caching Kernel Memory Allocator" - Jeff Bonwick
- "Buddy Memory Allocation" - Donald Knuth, TAOCP Vol 1
- Linux Kernel Memory Management Documentation

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*