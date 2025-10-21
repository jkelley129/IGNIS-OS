# IGNIS OS Architecture

## Overview

IGNIS is a 64-bit x86-64 hobby operating system built from scratch. It features a monolithic kernel design with modular driver architecture, running in higher-half kernel space with full memory management capabilities.

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
│  │              Memory Management                          │ │
│  │  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐ │ │
│  │  │   PMM    │  │   VMM    │  │  Heap Allocator     │ │ │
│  │  │(Physical)│  │(Virtual) │  │  (kmalloc/kfree)    │ │ │
│  │  └──────────┘  └──────────┘  └──────────────────────┘ │ │
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

#### Physical Memory Manager (PMM)
- **Bitmap-based allocation**: 1 bit per 4KB page
- **Location**: 3MB-4MB physical memory
- **Tracks**: 128MB of physical RAM (default QEMU config)
- **Functions**:
    - `pmm_alloc_page()` - Allocate single 4KB page
    - `pmm_alloc_pages(n)` - Allocate contiguous pages
    - `pmm_free_page()` - Free page
    - `pmm_mark_region_used()` - Reserve memory regions

#### Virtual Memory Manager (VMM)
- **4-level paging**: PML4 → PDPT → PD → PT
- **Page size**: 4KB standard, 2MB huge pages for kernel
- **Functions**:
    - `vmm_map_page()` - Map virtual to physical
    - `vmm_unmap_page()` - Unmap virtual address
    - `vmm_get_physical()` - Translate virtual to physical
    - `vmm_alloc_page()` - Allocate and map new page

#### Heap Allocator
- **Algorithm**: First-fit with coalescing
- **Features**:
    - `kmalloc()` - Allocate memory
    - `kfree()` - Free memory
    - `kcalloc()` - Zero-initialized allocation
    - `krealloc()` - Resize allocation
    - `kalloc_pages()` - Page-aligned allocation
- **Location**: 2MB-3MB physical (1MB heap)

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
2. **Priority 20**: PIT, Keyboard
3. **Priority 30**: Block device layer
4. **Priority 40**: ATA, NVMe drivers
5. **Priority 50**: Filesystems

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
- Built-in commands (27+ commands)
- File system operations
- Memory diagnostics
- Block device testing
- Kernel panic testing

#### Command Categories
- **System**: help, clear, about, uptime
- **Memory**: meminfo, memtest, pmminfo, pagetest
- **Filesystem**: ls, tree, touch, mkdir, rm, cat, write, cp
- **Block Devices**: lsblk, blkread, blkwrite, blktest
- **Drivers**: lsdrv

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
  compile → boot.o, kernel.o, drivers/*.o, etc.
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
- `memtest` - Heap allocator test
- `pagetest` - Page allocation test
- `blktest` - Block device I/O test

## Performance Characteristics

### Memory
- **Boot time heap**: 1MB (2MB-3MB physical)
- **Page allocation**: O(n) bitmap scan
- **kmalloc**: O(n) first-fit search
- **TLB**: Manual flush required after page table changes

### I/O
- **ATA PIO**: ~3-5 MB/s (polling mode)
- **VGA**: Direct memory access (fast)
- **Serial**: 38400 baud

## Future Enhancements

### Short(er) Term
- Better memory allocator (buddy/slab)
- User space processes
- System calls
- Process scheduler
- Inter-process communication (IPC)

### Long Term
- Graphical user interface (GUI)
- Network stack (TCP/IP)
- Audio support
- USB support
- SMP (multi-processor) support

## Code Organization
```
Total Lines of Code: ~8,000
├── Kernel Core:        ~500
├── Memory Management:  ~1,200
├── Drivers:           ~2,000
├── Filesystem:        ~1,500
├── Shell:             ~1,800
└── Other:             ~1,000
```

## Debugging Tips

### Common Issues
1. **Triple fault**: Usually paging issue, check page table setup
2. **General Protection Fault**: Invalid memory access or descriptor
3. **Page fault**: Invalid virtual address or missing mapping
4. **Freeze**: Likely interrupt issue, check IDT and PIC setup

### Debug Tools
- Serial output (`serial.log`)
- QEMU monitor (`Ctrl+Alt+2`)
- GDB debugging (`make run-gdb`)
- Kernel panic screen

## References

- [OSDev Wiki](https://wiki.osdev.org/)
- [Intel 64 and IA-32 Architectures Software Developer Manuals](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
- [GRUB Multiboot2 Specification](https://www.gnu.org/software/grub/manual/multiboot2/multiboot.html)
- [x86-64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*