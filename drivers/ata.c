#include "ata.h"
#include "block.h"
#include "../io/ports.h"
#include "../io/vga.h"
#include "../libc/string.h"
#include "../mm/memory.h"

typedef struct {
    uint16_t base;// Base I/O port
    uint16_t ctrl;//Control port
    uint8_t drive;//Device selection(master/slave)
} ata_device_data_t;

//Array of ATA block devices
static block_device_t ata_block_devices[4];
static ata_device_data_t ata_device_data[4];

static void ata_wait_busy(uint16_t base){
    while (inb(base + 7) & ATA_SR_BSY);
}

//Helper function to wait for data to be ready
static int ata_wait_drq(uint16_t base){
    uint8_t status;
    int timeout = 1000000;

    while(timeout--){
        status = inb(base + 7);
        if(status & ATA_SR_ERR) return -1;
        if(status & ATA_SR_DRQ) return 0;
    }
    return -2;
}

// Delay 400ns by reading 4 times(best approximation)
static void ata_io_wait(uint8_t base){
    for(uint8_t i = 0; i < 4; i++){
        inb(base + 7);
    }
}

static void ata_select_drive_and_lba(uint16_t base, uint8_t drive, uint64_t lba){
    // Select drive and set LBA mode
    outb(base + 6, (drive == ATA_MASTER ? 0xE0 : 0xF0) | ((lba >> 24) & 0x0F));
    outb(base + 2, 1);  // Sector count
    outb(base + 3, (uint8_t)lba);
    outb(base + 4, (uint8_t)(lba >> 8));
    outb(base + 5, (uint8_t)(lba >> 16));
    outb(base + 7, ATA_CMD_WRITE_PIO);
}

static int ata_read_block_op(block_device_t* dev, uint64_t lba, uint8_t* buffer){
    ata_device_data_t* ata_data = (ata_device_data_t*)dev->driver_data;
    uint16_t base = ata_data->base;

    //Wait for drive to be ready
    ata_wait_busy(base);

    //Select drive and set LBA mode
    ata_select_drive_and_lba(base, ata_data->drive, lba);

    // Wait for data to be ready
    if(ata_wait_drq(base) != 0){
        return -1;
    }

    //Read data
    uint16_t* buf16 = (uint16_t*)buffer;
    for(int i = 0; i < 256; i++){
        buf16[i] = inw(base);
    }

    return 0;
}

static int ata_write_block_op(block_device_t* dev, uint64_t lba, const uint8_t* buffer){
    ata_device_data_t* ata_data = (ata_device_data_t*)dev->driver_data;
    uint16_t base = ata_data->base;

    //Wait for drive to be ready
    ata_wait_busy(base);

    ata_select_drive_and_lba(base,ata_data->drive,lba);

    if(ata_wait_drq(base) != 0){
        return -1;
    }

    const uint16_t* buf16 = (const uint16_t*)buffer;
    for(int i = 0; i < 256; i++){
        outw(base, buf16[i]);
    }

    // Flush cache
    outb(base + 6, ata_data->drive == ATA_MASTER ? 0xE0 : 0xF0);
    outb(base + 7, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(base);

    return 0;
}

static int ata_flush_op(block_device_t* dev){
    ata_device_data_t* ata_data = (ata_device_data_t*)dev->driver_data;
    uint16_t base = ata_data->base;

    outb(base + 6, ata_data->drive == ATA_MASTER ? 0xE0 : 0xF0);
    outb(base + 7, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(base);

    return 0;
}

static const block_device_ops_t ata_ops = {
        .read_block = ata_read_block_op,
        .write_block = ata_write_block_op,
        .read_blocks = 0, // Use default implementation
        .write_blocks = 0,// Use default implementation
        .flush = ata_flush_op
};

static int ata_identify(uint8_t drive_num){
    ata_device_data_t* ata_data = &ata_device_data[drive_num];
    uint16_t base = ata_data->base;
    uint8_t drive_sel = ata_data->drive;

    // Select drive
    outb(base + 6, drive_sel);
    ata_io_wait(base);

    // Send IDENTIFY command
    outb(base + 7, ATA_CMD_IDENTIFY);
    ata_io_wait(base);

    // Check if drive exists
    uint8_t status = inb(base + 7);
    if (status == 0) {
        return -1;
    }

    //Wait for BSY to clear
    ata_wait_busy(base);

    // Check LBA mid and high - if non-zero, not ATA
    if (inb(base + 4) != 0 || inb(base + 5) != 0) {
        return -1;
    }

    // Wait for DRQ or ERR
    if (ata_wait_drq(base) != 0) {
        return -1;
    }

    // Read identification data
    uint16_t identify_data[256];
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(base);
    }

    // Get drive size (words 60-61 contain total sectors for LBA28)
    uint32_t sector_count = *((uint32_t*)&identify_data[60]);

    // Initialize block device structure
    block_device_t* block_dev = &ata_block_devices[drive_num];
    block_dev->type = BLOCK_TYPE_ATA;
    block_dev->block_count = sector_count;
    block_dev->block_size = ATA_SECTOR_SIZE;
    block_dev->present = 1;
    block_dev->driver_data = ata_data;
    block_dev->ops = &ata_ops;

    // Create device label
    char label[32];
    strcpy(label, "ATA");
    char num_str[8];
    uitoa(drive_num, num_str);
    strcat(label, num_str);
    strcpy(block_dev->label, label);

    block_register_device(block_dev);

    return 0;
}

void ata_init() {

    // Initialize ATA device data
    // Primary master
    ata_device_data[0].base = ATA_PRIMARY_DATA;
    ata_device_data[0].ctrl = ATA_PRIMARY_CONTROL;
    ata_device_data[0].drive = ATA_MASTER;

    // Primary slave
    ata_device_data[1].base = ATA_PRIMARY_DATA;
    ata_device_data[1].ctrl = ATA_PRIMARY_CONTROL;
    ata_device_data[1].drive = ATA_SLAVE;

    // Secondary master
    ata_device_data[2].base = ATA_SECONDARY_DATA;
    ata_device_data[2].ctrl = ATA_SECONDARY_CONTROL;
    ata_device_data[2].drive = ATA_MASTER;

    // Secondary slave
    ata_device_data[3].base = ATA_SECONDARY_DATA;
    ata_device_data[3].ctrl = ATA_SECONDARY_CONTROL;
    ata_device_data[3].drive = ATA_SLAVE;

    // Initialize block device structures
    for (int i = 0; i < 4; i++) {
        ata_block_devices[i].present = 0;
    }

    // Identify all drives and register them
    for (int i = 0; i < 4; i++) {
        if (ata_identify(i) == 0) {
            char num_str[32];
            vga_puts("  ");
            vga_puts(ata_block_devices[i].label);
            vga_puts(": Found (");
            uitoa(ata_block_devices[i].block_count / 2048, num_str);
            vga_puts(num_str);
            vga_puts(" MB)\n");
        }
    }
}