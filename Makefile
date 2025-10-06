# Makefile
CC = gcc
LD = ld
NASM = nasm

CFLAGS = -m32 -ffreestanding -nostdlib -nostdinc -I. -Idrivers -Iio
LDFLAGS = -m elf_i386 -T link.ld

BUILD_DIR = build

# Add input.o to the list of objects
all: $(BUILD_DIR)/kernel.elf

$(BUILD_DIR)/kernel.elf: $(BUILD_DIR)/boot.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/vga.o
	$(LD) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	$(NASM) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: io/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*

run:
	qemu-system-i386 -kernel $(BUILD_DIR)/kernel.elf
