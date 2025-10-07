CC = gcc
LD = ld
NASM = nasm
GRUB_CREATE_ISO = grub-mkrescue

# Changed to 64-bit flags
CFLAGS = -m64 -ffreestanding -nostdlib -nostdinc -fno-pie -mcmodel=large -mno-red-zone -I. -Idrivers -Iio
LDFLAGS = -m elf_x86_64 -T link.ld -nostdlib

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

all: build/ignis.iso

build/ignis.iso: iso
	$(GRUB_CREATE_ISO) -o $@ $^

iso: $(OBJS)
	$(LD) $(LDFLAGS) -o $@/boot/kernel.elf $^

$(BUILD_DIR)/kernel.o: kernel.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.o: boot.asm
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/idt_asm.o: idt.asm
	$(NASM) -f elf64 $< -o $@

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
	rm -rf $(BUILD_DIR)/* iso/boot/kernel.elf

run:
	qemu-system-x86_64 -cdrom $(BUILD_DIR)/ignis.iso