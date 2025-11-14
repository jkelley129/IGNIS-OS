# IGNIS Shell - Command Reference

## System Commands
| Command     | Usage         | Description                    |
|-------------|---------------|--------------------------------|
| `help`      | `help`        | Display all available commands |
| `about`     | `about`       | Show OS information            |
| `clear`     | `clear`       | Clear the screen               |
| `uptime`    | `uptime`      | Show system uptime             |
| `ticks`     | `ticks`       | Show PIT timer ticks           |
| `echo`      | `echo <text>` | Print text to screen           |
| `lsdrv`     | `lsdrv`       | List all registered drivers    |
| `panic`     | `panic <msg>` | Initiates kernel panic         |
| `panictest` | `panictest`   | Tests kernel panic macros      |
 | `ps`       | `ps`          | Print task list                |

## Memory Management Commands

### Overview Commands
| Command        | Usage          | Description                           |
|----------------|----------------|---------------------------------------|
| `meminfo`      | `meminfo`      | Legacy heap allocator statistics      |
| `pmminfo`      | `pmminfo`      | Physical Memory Manager statistics    |
| `buddyinfo`    | `buddyinfo`    | Buddy allocator statistics            |
| `slabinfo`     | `slabinfo`     | Slab allocator statistics (all caches)|

### Testing Commands
| Command     | Usage       | Description                        |
|-------------|-------------|------------------------------------|
| `memtest`   | `memtest`   | Test legacy heap allocator         |
| `pagetest`  | `pagetest`  | Test PMM page allocation           |
| `buddytest` | `buddytest` | Test buddy allocator alloc/free    |
| `slabtest`  | `slabtest`  | Test slab allocator alloc/free     |

### Memory Command Details

#### `buddyinfo`
Displays detailed buddy allocator statistics:
- Total memory managed (64 MB)
- Used and free memory
- Number of splits and merges performed
- Free blocks available at each order (0-11)
- Orders: 0=4KB, 1=8KB, 2=16KB, ..., 11=8MB

Example output:
```
=== Buddy Allocator Statistics ===
Total memory: 64 MB
Used memory:  1024 KB
Free memory:  64512 KB

Splits: 15  Merges: 8

Free blocks by order:
  Order 0 (4 KB): 128 blocks
  Order 1 (8 KB): 64 blocks
  Order 2 (16 KB): 32 blocks
  ...
```

#### `slabinfo`
Shows statistics for all slab caches:
- Cache name (e.g., kmalloc-32, kmalloc-64)
- Object size
- Objects per slab
- Active objects (currently allocated)
- Total slabs
- Total allocations and frees

Example output:
```
=== Slab Allocator Statistics ===

Cache: kmalloc-32
  Object size:    32 bytes
  Objects/slab:   128
  Active objects: 45
  Total slabs:    2
  Allocations:    152
  Frees:          107

Cache: kmalloc-64
  Object size:    64 bytes
  ...
```

#### `buddytest`
Performs buddy allocator tests:
1. Allocates blocks of various sizes (4KB, 16KB, 1MB)
2. Tests proper allocation
3. Frees blocks in specific order
4. Verifies buddy merging works correctly
5. Displays final statistics

Use this to verify buddy allocator is working correctly.

#### `slabtest`
Performs slab allocator tests:
1. Allocates 10 objects from 64-byte cache
2. Frees 5 objects
3. Re-allocates 5 objects (should reuse freed objects)
4. Frees all objects
5. Displays cache statistics

Use this to verify slab allocator is working correctly and reusing objects.

## File System Commands
| Command | Usage | Description |
|---------|-------|-------------|
| `ls` | `ls [path]` | List directory contents |
| `tree` | `tree [path]` | Show directory tree structure |
| `touch` | `touch <filename>` | Create new empty file |
| `mkdir` | `mkdir <dirname>` | Create new directory |
| `rm` | `rm <path>` | Remove file or directory |
| `cat` | `cat <filename>` | Display file contents |
| `write` | `write <filename> <text>` | Write text to file (overwrites) |
| `cp` | `cp <source> <dest>` | Copy file |
| `hexdump` | `hexdump <filename>` | Show file in hexadecimal format |

### File System Command Details

#### `ls [path]`
Lists contents of directory at `path` (default: current directory `/`).

Example:
```
ignis$ ls /
[DIR]  home
[DIR]  etc
[FILE] test.txt (42 bytes)
```

#### `tree [path]`
Shows recursive directory tree starting at `path`.

Example:
```
ignis$ tree /
/
├── home/
│   ├── user/
│   │   └── document.txt
│   └── logs/
└── etc/
    └── config.txt
```

#### `cat <filename>`
Displays the contents of a text file.

Example:
```
ignis$ cat /test.txt
Hello, IGNIS OS!
This is a test file.
```

#### `write <filename> <text>`
Writes text to a file, creating it if it doesn't exist or overwriting if it does.

Example:
```
ignis$ write /greeting.txt Hello World
File written: 11 bytes
```

## Block Device Commands
| Command | Usage | Description |
|---------|-------|-------------|
| `lsblk` | `lsblk` | List all block devices |
| `blkread` | `blkread <id> <lba>` | Read block from device |
| `blkwrite` | `blkwrite <id> <lba> <data>` | Write block to device |
| `blktest` | `blktest <id>` | Test device read/write operations |

### Block Device Command Details

#### `lsblk`
Lists all detected block devices with their properties.

Example:
```
Block Devices:
  [0] ATA Primary Master - 64 MB (131072 blocks)
  [1] Not present
  [2] Not present
  [3] Not present
```

#### `blkread <id> <lba>`
Reads a single 512-byte block from device `id` at Logical Block Address `lba`.

Example:
```
ignis$ blkread 0 0
Reading block 0 from device 0...
[Displays 512 bytes in hexadecimal]
```

#### `blkwrite <id> <lba> <data>`
Writes data to device `id` at block `lba`. Data is padded to 512 bytes.

Example:
```
ignis$ blkwrite 0 100 TestData
Writing to block 100 on device 0...
Write successful
```

#### `blktest <id>`
Performs comprehensive read/write test on device:
1. Writes test pattern to block 100
2. Reads it back
3. Verifies data integrity
4. Reports success or failure

## Command Usage Tips

### Memory Commands
- Run `buddyinfo` and `slabinfo` regularly to monitor memory usage
- Use `buddytest` and `slabtest` after system changes to verify allocators
- Check `pmminfo` to see raw page availability
- `meminfo` shows the legacy heap (mostly unused now)

### File System Commands
- Use `tree` for visual directory structure
- Use `ls` for detailed file information
- Paths always start with `/`
- File operations work only in RAM (RAMFS)

### Block Device Commands
- Always check `lsblk` first to see available devices
- Device IDs start at 0
- LBA (Logical Block Address) starts at 0
- Block size is always 512 bytes
- Be careful with `blkwrite` - it modifies disk data!

## Notes
- All commands are **case-sensitive**
- Multiple arguments are **space-separated**
- File paths use **/** as separator
- Default directory is **/** (root)
- Device IDs are **0-3** for ATA devices
- LBA values are **decimal numbers**
- Commands provide helpful error messages
- Use `help` to see quick command list

## Error Messages

Common error messages you might see:

### Memory Errors
- `Out of memory` - Buddy allocator exhausted, check `buddyinfo`
- `Allocation failed` - Slab cache or buddy allocator couldn't fulfill request
- `Invalid address` - Attempting to free invalid pointer

### File System Errors
- `File not found` - Path doesn't exist
- `Not a directory` - Trying to list a file as directory
- `Is a directory` - Trying to read/write a directory as file
- `Already exists` - File/directory name collision

### Block Device Errors
- `Device not found` - Invalid device ID
- `Read failed` - Hardware error reading block
- `Write failed` - Hardware error writing block
- `Invalid LBA` - Block address out of range

---

*Last Updated: January 2025*
*IGNIS OS Version: 0.0.01*