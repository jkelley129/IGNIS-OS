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
SRC_DIRS = . boot interrupts drivers drivers/disks io console shell mm mm/allocators scheduler fs fs/filesystems libc error_handling

# Disk images
ATA_DISK = $(OUTPUT_DIR)/ata_disk.img
NVME_DISK = $(OUTPUT_DIR)/nvme_disk.img

# ============================================================================
# AUTOMATIC SOURCE FILE DISCOVERY
# ============================================================================

# Find all .c files in source directories
C_SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

# Find all .asm files in source directories
ASM_SOURCES = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.asm))

# Generate object file names
# Add 'asm_' prefix to assembly object files to avoid naming collisions
C_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(notdir $(C_SOURCES)))
ASM_OBJS = $(patsubst %.asm,$(BUILD_DIR)/asm_%.o,$(notdir $(ASM_SOURCES)))

# All object files
OBJS = $(C_OBJS) $(ASM_OBJS)

# Create a list of VPATH directories for make to search
VPATH = $(SRC_DIRS)

# ============================================================================
# DEFAULT TARGET
# ============================================================================

all: $(OUTPUT_DIR)/ignis.iso

# ============================================================================
# BUILD TARGETS
# ============================================================================

# Build ISO
$(OUTPUT_DIR)/ignis.iso: iso | $(OUTPUT_DIR)
	$(GRUB_CREATE_ISO) -o $@ $^

iso: $(OBJS)
	@mkdir -p iso/boot
	$(LD) $(LDFLAGS) -o iso/boot/kernel.elf $^

# Generic rule for compiling C files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Generic rule for assembling ASM files (with asm_ prefix to avoid collisions)
$(BUILD_DIR)/asm_%.o: %.asm | $(BUILD_DIR)
	$(NASM) -f elf64 $< -o $@

# Create directories
$(BUILD_DIR):
	@mkdir -p $@

$(OUTPUT_DIR):
	@mkdir -p $@

# ============================================================================
# DISK IMAGE TARGETS
# ============================================================================

# Create both disk images
disks: | $(OUTPUT_DIR)
	@echo "Creating disk images..."
	@qemu-img create -f raw $(ATA_DISK) 512M
	@qemu-img create -f raw $(NVME_DISK) 1G
	@echo "Disk images created successfully!"
	@echo "  - $(ATA_DISK): 512 MB"
	@echo "  - $(NVME_DISK): 1 GB"

disk-ata: | $(OUTPUT_DIR)
	@echo "Creating ATA disk image..."
	@qemu-img create -f raw $(ATA_DISK) 512M
	@echo "Disk created successfully"

disk-nvme: | $(OUTPUT_DIR)
	@echo "Creating NVMe disk image..."
	@qemu-img create -f raw $(NVME_DISK) 1G
	@echo "Disk created successfully"

# Create larger disk images
disks-large: | $(OUTPUT_DIR)
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

# Run without any disks
run: $(OUTPUT_DIR)/ignis.iso
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso -serial file:serial.log

# Run with ATA disk only
run-ata: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		echo "ATA disk not found. Creating..."; \
		$(MAKE) disk-ata; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-serial file:serial.log

# Run with NVMe disk only
run-nvme: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(NVME_DISK) ]; then \
		echo "NVMe disk not found. Creating..."; \
		$(MAKE) disk-nvme; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(NVME_DISK),if=none,id=nvm \
		-device nvme,serial=deadbeef,drive=nvm \
		-serial file:serial.log

# Run with both ATA and NVMe disks
run-full: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		echo "ATA disk not found. Creating..."; \
		$(MAKE) disk-ata; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		echo "NVMe disk not found. Creating..."; \
		$(MAKE) disk-nvme; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-drive file=$(NVME_DISK),if=none,id=nvm,format=raw \
		-device nvme,serial=deadbeef,drive=nvm \
		-m 512M \
		-serial file:serial.log

# Run with debug output
run-debug: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		$(MAKE) disk-ata; \
	fi
	@if [ ! -f $NVME_DISK ]; then \
  		$(MAKE) disk-nvme; \
  	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-drive file=$(NVME_DISK),if=none,id=nvm,format=raw \
		-device nvme,serial=deadbeef,drive=nvm \
		-d guest_errors,int \
		-D qemu.log \
		-no-reboot \
		-no-shutdown \
		-serial file:serial.log

# Run with GDB debugging support
run-gdb: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		$(MAKE) disk-ata; \
	fi
	@echo "Starting QEMU with GDB support on port 1234"
	@echo "In another terminal, run: gdb iso/boot/kernel.elf"
	@echo "Then in GDB: target remote localhost:1234"
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-s -S -serial file:serial.log

# Run with multiple NVMe devices
run-multi-nvme: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		$(MAKE) disk-ata; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		$(MAKE) disk-nvme; \
	fi
	@if [ ! -f $(OUTPUT_DIR)/nvme_disk2.img ]; then \
		qemu-img create -f raw $(OUTPUT_DIR)/nvme_disk2.img 1G; \
	fi
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide \
		-drive file=$(NVME_DISK),if=none,id=nvm1,format=raw \
		-device nvme,serial=nvme001,drive=nvm1 \
		-drive file=$(OUTPUT_DIR)/nvme_disk2.img,if=none,id=nvm2,format=raw \
		-device nvme,serial=nvme002,drive=nvm2 \
		-m 512M -serial file:serial.log

# Run in snapshot mode (changes not saved)
run-snapshot: $(OUTPUT_DIR)/ignis.iso
	@if [ ! -f $(ATA_DISK) ]; then \
		$(MAKE) disk-ata; \
	fi
	@if [ ! -f $(NVME_DISK) ]; then \
		$(MAKE) disk-nvme; \
	fi
	@echo "Running in snapshot mode - changes will NOT be saved"
	$(QEMU) -cdrom $(OUTPUT_DIR)/ignis.iso \
		-drive file=$(ATA_DISK),format=raw,if=ide,snapshot=on \
		-drive file=$(NVME_DISK),if=none,id=nvm,format=raw,snapshot=on \
		-device nvme,serial=deadbeef,drive=nvm -serial file:serial.log

# ============================================================================
# UTILITY TARGETS
# ============================================================================

# Show detected source files (useful for debugging the Makefile)
show-sources:
	@echo "=== Detected Source Files ==="
	@echo "\nC Sources:"
	@echo "$(C_SOURCES)" | tr ' ' '\n'
	@echo "\nASM Sources:"
	@echo "$(ASM_SOURCES)" | tr ' ' '\n'
	@echo "\nObject Files:"
	@echo "$(OBJS)" | tr ' ' '\n'
	@echo "\nVPATH:"
	@echo "$(VPATH)" | tr ' ' '\n'

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
clean-objs:
	rm -rf $(BUILD_DIR)/*

# Clean disk images
clean-disks:
	@echo "Removing disk images..."
	rm -f $(ATA_DISK) $(NVME_DISK) $(OUTPUT_DIR)/nvme_disk2.img
	rm -f *.img *.qcow2
	rm -f qemu.log
	@echo "Disk images removed"

clean-logs:
	rm -f serial.log qemu.log

# Clean everything including backups
clean-all: clean clean-disks
	rm -f *.backup *.log

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
	@echo "  make show-sources   - Show all detected source files"
	@echo "  make help           - Show this help message"
	@echo ""
	@echo "Quick Start:"
	@echo "  1. make disks       - Create disk images"
	@echo "  2. make             - Build IGNIS"
	@echo "  3. make run-full    - Run with all devices"

.PHONY: all clean clean-objs clean-disks clean-logs clean-all disks disks-large diskinfo \
        run run-ata run-nvme run-full run-debug run-gdb run-multi-nvme \
        run-snapshot dump-ata dump-nvme backup-disks restore-disks help \
        show-sources disk-ata disk-nvme