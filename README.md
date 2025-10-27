# Ignis OS
## What is Ignis?
Ignis is an open source hobby OS meant to gain experience with low level concepts and implementations. This is a learning project, and I hope to expand it to learn more about all of the concepts. I am using a variety of tools to facilitate my learning, including CPU documentation, hardware docs, and some genAI tools to help me learn. <br>
This is **NOT** a Linux distribution or UNIX system. It is currently just a kernel(userspace in the future), built from nothing.

## Technical Description
- It is designed for 64-bit systems for the x86 instruction set
- It uses a custom Multiboot2 compatible boot header with GRUB2
- For testing and developing, it is emulated using [QEMU](https://qemu.org)
- For a full feature list, look [here](#current-features)

## Installation
### Required Build Dependencies
- A modern C compiler(GNU gcc recommended)
- A linker (GNU ld recommended)
- An assembler (NASM recommended)
- grub-mkrescue
- make

### Required Emulator
- You must install the QEMU emulator to for this project.
- It is expected by `make run` to emulate the ignis.iso image created by `make`

### Getting Started
**Steps**:
- Install build dependencies.
- Clone the repo with `git clone https://github.com/jkelley129/IGNIS-OS.git && cd IGNIS-OS`
- Compile and package the project with `make` or `make all`
  - The `Makefile` creates a ignis.iso file in iso/boot/ that is emulated by QEMU
- If running with a disk image, run `make disks`, `make disk-ata`, or `make disk-nvme`. Note that only ATA is working at the moment
- Run the QEMU emulator with `make run` if not working with disks, else use `make run-full`, `make run-ata`, or `make run-nvme` for your disk

## Current Features
Features in rough chronological order of implementation
- Console layer for outputting text to the screen via VGA
- Interrupt Descriptor Table for handling interrupts(keypresses, hardware interrupts)
- Interactive kernel-level shell for debugging and functionality
- Programmable Interval Timer with interrupt handling
- Custom memory allocator, with a simple free list implementation
- In-memory filesystem with simple utilities(create, list, copy, delete, etc.)
- Agnostic block device I/O layer
- Block device drivers
  - ATA
- Error handling with enums(`kerr_t`) and helper functions/macros
- Generic driver registration system with features like priority and dependencies
- Serial driver for external logging
- Page-aligned memory allocator
- Kernel Panic
- Sophisticated memory allocators
  - Buddy allocator
  - Slab allocator
  - Unified allocator interface([mm/allocators/kmalloc.c](https://github.com/jkelley129/IGNIS-OS/blob/main/mm/allocators/kmalloc.c))

## TODO
- *(TEST)* Disk drivers for persistent storage(on branch [disk_testing](https://github.com/jkelley129/IGNIS-OS/tree/disk_testing)) **[ ON HOLD ]**
- Implementation of an on-disk filesystem
- More device drivers for more devices
- User space with limited permissions
- Support for executing user programs
- User applications(text editor, clock, stopwatch, utilities)
- Audio integration
- Graphical User Interface(maybe)

## Contributing
### Issues
View .github/ISSUE_TEMPLATES/ for detailed issue templates. Issues are always welcome, as long as they fit the template, are reproducable, and fit the scale and theme of the project
### Pull Requests
Because of the nature of this repository, I would ask anyone to refrain from large PRs implementing a lot of new logic. I want to make IGNIS as best as possible, but I want to learn and implement the core features by myself. If you have code you want to contribute, feel free to make an issue or PR. Just know that I might not merge it if I don't fully understand it. Thank you for your interest!
