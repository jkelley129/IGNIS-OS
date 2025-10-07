# Makefile
CC = gcc
LD = ld
NASM = nasm

CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -I. -Idrivers -Iio
LDFLAGS = -m elf_i386 -T link.ld

BUILD_DIR = build

# Updated object file list
OBJS = $(BUILD_DIR)/boot.o \
       $(BUILD_DIR)/kernel.o \
       $(BUILD_DIR)/vga.o \
       $(BUILD_DIR)/idt.o \
       $(BUILD_DIR)/keyboard.o \
       $(BUILD_DIR)/idt_asm.o \
       $(BUILD_DIR)/shell.o \
       $(BUILD_DIR)/string.o

all: $(BUILD_DIR)/kernel.elf

$(BUILD_DIR)/kernel.elf: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.o: boot.asm
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/idt_asm.o: idt.asm
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/vga.o: io/vga.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: io/idt.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: shell.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: libc/string.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -rf $(BUILD_DIR)/*

run:
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf