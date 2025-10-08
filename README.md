# Ignis OS
## What is Ignis?
Ignis is an open source hobby OS meant to gain experience with low level concepts and implementations. Note that this is not a Linux distribution, but rather a kernel written from scratch

## Technical Description
- It is designed for 64 bit systems for the x86 instruction set
- It uses a custom Multiboot2 compatible boot header with GRUB2
- For testing and developing, it is emulated using [QEMU](https://qemu.org)

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
- Run the QEMU emulator with `make run` or `make run_debug`

## Current Features
Features in rough chronological order of implementation
- VGA for outputting text to the screen
- Interrupt Descriptor Table for handling interrupts(keypresses, hardware interrupts)
- Interactive kernel-level shell for debugging and functionality
- Programmable Interval Timer with interrupt handling
- Custom memory allocator, with a simple free list implementation
- In-memory filesystem with simple utilities(create, list, copy, delete, etc.)

## Features I hope to add
- Disk driver for persistent storage
- Implementation of an on-disk filesystem
- More standard filesystem(FAT32, or other)
- More device drivers for more devices
- Audio integration
- More sophisticated memory allocator(buddy allocator, slab allocator)
- User space with limited permissions
- User applications(text editor, clock, stopwatch, utilities)
- Support for executing user programs
- Graphical User Interface(maybe)

## Contributing
### Just make a pull request
