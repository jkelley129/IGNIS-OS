CC = gcc
LD = ld
NASM = nasm
GRUB_CREATE_ISO = grub-mkrescue
QEMU = qemu-system-x86_64

# Compiler and linker flags
CFLAGS = -m64 -ffreestanding -nostdlib -nostdinc -fno-pie -mcmodel=large -mno-red-zone -I. -Idrivers -Iio
LDFLAGS = -m elf_x86_64 -T link.ld -nostdlib

# Directories
BUILD_DIR = build
OUTPUT_DIR = dist

# Disk images
ATA_DISK = $(OUTPUT_DIR)/ata_disk.img
NVME_DISK = $(OUTPUT_DIR)/nvme_disk.img

# Object files
OBJS = $(BUILD_DIR)/boot.o \
       $(BUILD_DIR)/kernel.o \
       $(BUILD_DIR)/driver.o \
       $(BUILD_DIR)/vga.o \
       $(BUILD_DIR)/console.o \
       $(BUILD_DIR)/idt.o \
       $(BUILD_DIR)/keyboard.o \
       $(BUILD_DIR)/pit.o \
       $(BUILD_DIR)/idt_asm.o \
       $(BUILD_DIR)/shell.o \
       $(BUILD_DIR)/string.o \
       $(BUILD_DIR)/memory.o \
       $(BUILD_DIR)/vfs.o \
       $(BUILD_DIR)/ramfs.o \
       $(BUILD_DIR)/block.o \
       $(BUILD_DIR)/ata.o \
       $(BUILD_DIR)/nvme.o \
       $(BUILD_DIR)/errno.o

# Default target
all: $(OUTPUT_DIR)/ignis.iso

# Build ISO
$(OUTPUT_DIR)/ignis.iso: iso | $(OUTPUT_DIR)
	$(GRUB_CREATE_ISO) -o $@ $^

iso: $(OBJS)
	$(LD) $(LDFLAGS) -o $@/boot/kernel.elf $^

# Compilation rules
$(BUILD_DIR)/kernel.o: kernel.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/memory.o: mm/memory.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/boot.o: boot.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/idt_asm.o: interrupts/idt.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

$(BUILD_DIR)/driver.o: drivers/driver.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vga.o: io/vga.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/console.o: console/console.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/idt.o: interrupts/idt.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: drivers/keyboard.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pit.o: drivers/pit.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/block.o: drivers/block.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ata.o: drivers/disks/ata.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/nvme.o: drivers/disks/nvme.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/errno.o: error_handling/errno.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: shell/shell.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/vfs.o: fs/vfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ramfs.o: fs/filesystems/ramfs.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: libc/string.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(BUILD_DIR):
	@mkdir -p $@

$(OUTPUT_DIR):
	@mkdir -p $@

# ============================================================================
# DISK IMAGE TARGETS
# ============================================================================

# Create both disk images
disks:
	@echo "Creating disk images..."
	@qemu-img create -f raw $(ATA_DISK) 512M
	@qemu-img create -f raw $(NVME_DISK) 1G
	@echo "Disk images created successfully!"
	@echo "  - $(ATA_DISK): 512 MB"
	@echo "  - $(NVME_DISK): 1 GB"

disk-ata:
	@echo "Creating ATA disk image..."
	@qemu-img create -f raw $(ATA_DISK) 512M
	@echo "Disk created successfully"

disk-nvme:
	@echo "Creating NVMe disk image..."
	@qemu-img create -f raw $(NVMe_DISK) 1G
	@echo "Disk created successfully"

# Create larger disk images
disks-large:
	@echo "Creating large disk images..."
	@qemu-img create -f raw $(ATA_DISK) 2G
	@qemu-img create -f raw $(NVME_DISK) 4G
	@echo "✓ Large disk images created successfully!"
	@echo "  - $(ATA_DISK): 2 GB"
	@echo "  - $(NVME_DISK): 4 GB"

# Show disk information
diskinfo:
	@echo "=== Disk Image Information ==="
	@if [ -f $(ATA_DISK) ]; then \
		echo "\nATA Disk:"; \
		qemu-img info $(ATA_DISK); \
	else \
		echo "\n$(ATA_DISK) not found. Run 'make disks' to create it."; \
	fi
	@if [ -f $(NVME_DISK) ]; then \
		echo "\nNVMe Disk:"; \
		qemu-img info $(NVME_DISK); \
	else \
		echo "\n$(NVME_DISK) not found. Run 'make disks' to create it."; \
	fi

# ============================================================================
# QEMU RUN TARGETS
# ============================================================================

# Run without any disks (original behavior)
run:
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso

# Run with ATA disk only
run-ata: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		echo "ATA disk not found. Creating..."; \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide

# Run with NVMe disk only
run-nvme: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(NVME_DISK) ]; then \
		echo "NVMe disk not found. Creating..."; \
		qemu-img create -f raw $(NVME_DISK) 1G; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(NVME_DISK),if=none,id=nvm \
		-device nvme,serial=deadbeef,drive=nvm

# Run with both ATA and NVMe disks
run-full: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		echo "ATA disk not found. Creating..."; \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		echo "NVMe disk not found. Creating..."; \
		qemu-img create -f raw $(NVME_DISK) 1G; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-drive file=$(NVME_DISK),if=none,id=nvm,format=raw \
		-device nvme,serial=deadbeef,drive=nvm \
		-m 512M

# Run with debug output
run-debug: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-d guest_errors,int \
		-D qemu.log \
		-no-reboot \
		-no-shutdown

# Run with GDB debugging support
run-gdb: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	@echo "Starting QEMU with GDB support on port 1234"
	@echo "In another terminal, run: gdb iso/boot/kernel.elf"
	@echo "Then in GDB: target remote localhost:1234"
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-s -S

# Run with multiple NVMe devices
run-multi-nvme: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		qemu-img create -f raw $(NVME_DISK) 1G; \
	fi
	@if [ ! -f nvme_disk2.img ]; then \
		qemu-img create -f raw nvme_disk2.img 1G; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-drive file=$(NVME_DISK),if=none,id=nvm1,format=raw \
		-device nvme,serial=nvme001,drive=nvm1 \
		-drive file=nvme_disk2.img,if=none,id=nvm2,format=raw \
		-device nvme,serial=nvme002,drive=nvm2 \
		-m 512M

# Run in snapshot mode (changes not saved)
run-snapshot: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		qemu-img create -f raw $(ATA_DISK) 512M; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		qemu-img create -f raw $(NVME_DISK) 1G; \
	fi
	@echo "Running in snapshot mode - changes will NOT be saved"
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide,snapshot=on \
		-drive file=$(NVME_DISK),if=none,id=nvm,format=raw,snapshot=on \
		-device nvme,serial=deadbeef,drive=nvm

# ============================================================================
# UTILITY TARGETS
# ============================================================================

# Dump first sector of ATA disk
dump-ata:
	@if [ -f $(ATA_DISK) ]; then \
		echo "First sector of $(ATA_DISK):"; \
		dd if=$(ATA_DISK) bs=512 count=1 2>/dev/null | hexdump -C | head -n 32; \
	else \
		echo "$(ATA_DISK) not found."; \
	fi

# Dump first sector of NVMe disk
dump-nvme:
	@if [ -f $(NVME_DISK) ]; then \
		echo "First sector of $(NVME_DISK):"; \
		dd if=$(NVME_DISK) bs=512 count=1 2>/dev/null | hexdump -C | head -n 32; \
	else \
		echo "$(NVME_DISK) not found."; \
	fi

# Backup disk images
backup-disks:
	@echo "Backing up disk images..."
	@if [ -f $(ATA_DISK) ]; then \
		cp $(ATA_DISK) $(ATA_DISK).backup; \
		echo "✓ Backed up $(ATA_DISK)"; \
	fi
	@if [ -f $(NVME_DISK) ]; then \
		cp $(NVME_DISK) $(NVME_DISK).backup; \
		echo "✓ Backed up $(NVME_DISK)"; \
	fi

# Restore disk images from backup
restore-disks:
	@echo "Restoring disk images from backup..."
	@if [ -f $(ATA_DISK).backup ]; then \
		cp $(ATA_DISK).backup $(ATA_DISK); \
		echo "✓ Restored $(ATA_DISK)"; \
	else \
		echo "✗ No backup found for $(ATA_DISK)"; \
	fi
	@if [ -f $(NVME_DISK).backup ]; then \
		cp $(NVME_DISK).backup $(NVME_DISK); \
		echo "✓ Restored $(NVME_DISK)"; \
	else \
		echo "✗ No backup found for $(NVME_DISK)"; \
	fi

# ============================================================================
# CLEAN TARGETS
# ============================================================================

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)/* $(OUTPUT_DIR)/* iso/boot/kernel.elf

# Clean object files only
clean_objs:
	rm -rf $(BUILD_DIR)/*

# Clean disk images
clean-disks:
	@echo "Removing disk images..."
	rm -f $(ATA_DISK) $(NVME_DISK) nvme_disk2.img
	rm -f *.img *.qcow2
	rm -f qemu.log
	@echo "✓ Disk images removed"

# Clean everything including backups
clean-all: clean clean-disks
	rm -f *.backup

# ============================================================================
# HELP TARGET
# ============================================================================

help:
	@echo "IGNIS OS Makefile"
	@echo "================="
	@echo ""
	@echo "Build Targets:"
	@echo "  make                - Build IGNIS OS ISO"
	@echo "  make all            - Same as 'make'"
	@echo "  make clean          - Clean build artifacts"
	@echo ""
	@echo "Disk Management:"
	@echo "  make disks          - Create 512MB ATA + 1GB NVMe disks"
	@echo "  make disks-large    - Create 2GB ATA + 4GB NVMe disks"
	@echo "  make diskinfo       - Show disk image information"
	@echo "  make dump-ata       - Dump first sector of ATA disk"
	@echo "  make dump-nvme      - Dump first sector of NVMe disk"
	@echo "  make backup-disks   - Backup all disk images"
	@echo "  make restore-disks  - Restore disk images from backup"
	@echo "  make clean-disks    - Remove all disk images"
	@echo ""
	@echo "Run Targets:"
	@echo "  make run            - Run IGNIS without disks"
	@echo "  make run-ata        - Run with ATA disk only"
	@echo "  make run-nvme       - Run with NVMe disk only"
	@echo "  make run-full       - Run with both ATA and NVMe disks"
	@echo "  make run-debug      - Run with debug output"
	@echo "  make run-gdb        - Run with GDB debugging support"
	@echo "  make run-multi-nvme - Run with multiple NVMe devices"
	@echo "  make run-snapshot   - Run without saving changes"
	@echo ""
	@echo "Utility:"
	@echo "  make help           - Show this help message"
	@echo ""
	@echo "Quick Start:"
	@echo "  1. make disks       - Create disk images"
	@echo "  2. make             - Build IGNIS"
	@echo "  3. make run-full    - Run with all devices"

.PHONY: all clean clean_objs clean-disks clean-all disks disks-large diskinfo \
        run run-ata run-nvme run-full run-debug run-gdb run-multi-nvme \
        run-snapshot dump-