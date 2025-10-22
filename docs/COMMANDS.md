# IGNIS Shell - Command Sheet

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
| `panictest` | `lsdrv`       | Tests kernel panic macros      |

## Memory Commands
| Command     | Usage       | Description                   |
|-------------|-------------|-------------------------------|
| `meminfo`   | `meminfo`   | Show memory statistics        |
| `memtest`   | `memtest`   | Run memory allocator test     |
| `pmminfo`   | `pmminfo`   | Show PMM stats                |
| `pagetest`  | `pagetest`  | Test page memory allocation   |
| `buddyinfo` | `buddyinfo` | Display buddy allocator info  |
| `buddytest` | `buddytest` | Test buddy allocator          |
| `slabinfo`  | `slabinfo`  | Display slab allocator info   |
| `slabtest`  | `slabtest`  | Test slab allocator           |

## File System Commands
| Command | Usage | Description |
|---------|-------|-------------|
| `ls` | `ls [path]` | List directory contents |
| `tree` | `tree [path]` | Show directory tree |
| `touch` | `touch <filename>` | Create new file |
| `mkdir` | `mkdir <dirname>` | Create new directory |
| `rm` | `rm <path>` | Remove file/directory |
| `cat` | `cat <filename>` | Display file contents |
| `write` | `write <filename> <text>` | Write text to file |
| `cp` | `cp <source> <dest>` | Copy file |
| `hexdump` | `hexdump <filename>` | Show file in hex format |

## Block Device Commands
| Command | Usage | Description |
|---------|-------|-------------|
| `lsblk` | `lsblk` | List all block devices |
| `blkread` | `blkread <id> <lba>` | Read block from device |
| `blkwrite` | `blkwrite <id> <lba> <data>` | Write block to device |
| `blktest` | `blktest <id>` | Test device read/write |

## Notes
- All commands are **case-sensitive**
- Multiple arguments are **space-separated**
- File paths use **/** as separator
- Default directory is **/** (root)
- Device IDs start at **0**
- LBA values are **decimal numbers**