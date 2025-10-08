CC = gcc
LD = ld
NASM = nasm
GRUB_CREATE_ISO = grub-mkrescue

# Changed to 64-bit flags
CFLAGS = -m64 -ffreestanding -nostdlib -nostdinc -fno-pie -mcmodel=large -mno-red-zone -I. -Idrivers -Iio
LDFLAGS = -m elf_x86_64 -T link.ld -nostdlib

BUILD_DIR = build
OUTPUT_DIR = dist

# Updated object file list
OBJS = $(BUILD_DIR)/boot.o \
       $(BUILD_DIR)/kernel.o \
       $(BUILD_DIR)/vga.o \
       $(BUILD_DIR)/idt.o \
       $(BUILD_DIR)/keyboard.o \
       $(BUILD_DIR)/pit.o \
       $(BUILD_DIR)/idt_asm.o \
       $(BUILD_DIR)/shell.o \
       $(BUILD_DIR)/string.o \
       $(BUILD_DIR)/memory.o \
       $(BUILD_DIR)/vfs.o

all: $(OUTPUT_DIR)/ignis.iso

$(OUTPUT_DIR)/ignis.iso: iso | $(OUTPUT_DIR)
	$(GRUB_CREATE_ISO) -o $@ $^

iso: $(OBJS)
	$(LD) $(LDFLAGS) -o $@/boot/kernel.elf $^

$(BUILD_DIR)/kernel.o: kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memory.o: mm/memory.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.o: boot.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/idt_asm.o: idt.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/vga.o: io/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: io/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pit.o: drivers/pit.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: fs/vfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: libc/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $@

$(OUTPUT_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)/* $(OUTPUT_DIR)/* iso/boot/kernel.elf

clean_objs:
	rm -rf $(BUILD_DIR)/*

run:
	qemu-system-x86_64 -cdrom $(OUTPUT_DIR)/ignis.iso