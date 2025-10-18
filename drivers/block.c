#include "block.h"
#include "console/console.h"
#include "libc/string.h"
#include "error_handling/errno.h"
#include "mm/memory.h"
#include "driver.h"

// Block manager structure (hidden from other files)
struct block_manager {
    block_device_t* devices[MAX_BLOCK_DEVICES];
    uint8_t device_count;
};

// Global block manager instance
static block_manager_t* g_block_manager = NULL;

// Forward declaration of driver init function
static kerr_t block_driver_init(driver_t* drv);

// Driver structure for block device layer
static driver_t block_layer_driver = {
    .name = "Block Layer",
    .type = DRIVER_TYPE_BLOCK,
    .version = 1,
    .priority = 30,
    .init = block_driver_init,
    .cleanup = NULL,
    .depends_on = "",
    .driver_data = NULL
};

// Driver initialization function
static kerr_t block_driver_init(driver_t* drv) {
    g_block_manager = (block_manager_t*)kmalloc(sizeof(block_manager_t));
    if (!g_block_manager) return E_NOMEM;

    for (int i = 0; i < MAX_BLOCK_DEVICES; i++) {
        g_block_manager->devices[i] = NULL;
    }
    g_block_manager->device_count = 0;

    return E_OK;
}

kerr_t block_register(void) {
    return driver_register(&block_layer_driver);
}

block_manager_t* block_get_manager(void) {
    return g_block_manager;
}

int block_register_device(block_device_t* device) {
    if (!g_block_manager) return -1;
    if (g_block_manager->device_count >= MAX_BLOCK_DEVICES) return -1;

    device->id = g_block_manager->device_count;
    g_block_manager->devices[g_block_manager->device_count] = device;
    g_block_manager->device_count++;

    return device->id;
}

block_device_t* block_get_device(uint8_t id) {
    if (!g_block_manager) return NULL;
    if (id >= g_block_manager->device_count) return NULL;
    return g_block_manager->devices[id];
}

uint8_t block_get_device_count(void) {
    if (!g_block_manager) return 0;
    return g_block_manager->device_count;
}

void block_list_devices(void) {
    if (!g_block_manager) {
        console_puts("Block manager not initialized\n");
        return;
    }

    console_puts("\n=== Block Devices ===\n");

    if (g_block_manager->device_count == 0) {
        console_puts("No block devices found\n");
        return;
    }

    for (uint8_t i = 0; i < g_block_manager->device_count; i++) {
        block_device_t* dev = g_block_manager->devices[i];

        if (dev && dev->present) {
            char num_str[32];

            console_puts("Device ");
            uitoa(dev->id, num_str);
            console_puts(num_str);
            console_puts(": ");
            console_puts(dev->label);
            console_puts(" (");

            switch (dev->type) {
                case BLOCK_TYPE_ATA:
                    console_puts("ATA");
                    break;
                case BLOCK_TYPE_AHCI:
                    console_puts("AHCI");
                    break;
                case BLOCK_TYPE_NVME:
                    console_puts("NVME");
                    break;
                case BLOCK_TYPE_RAMDISK:
                    console_puts("RAM Disk");
                    break;
                default:
                    console_puts("Unknown");
            }

            console_puts(") - ");

            uint64_t size_mb = (dev->block_count * dev->block_size) / (1024 * 1024);
            uitoa(size_mb, num_str);
            console_puts(num_str);
            console_puts(" MB\n");
        }
    }
    console_putc('\n');
}

// Generic I/O operations
int block_read(uint8_t device_id, uint64_t lba, uint8_t* buffer) {
    block_device_t* dev = block_get_device(device_id);
    if (!dev) return E_NOTFOUND;
    if (!dev->present || !dev->ops || !dev->ops->read_block) return E_INVALID;
    if (lba >= dev->block_count) return E_EXISTS;

    return dev->ops->read_block(dev, lba, buffer);
}

int block_write(uint8_t device_id, uint64_t lba, const uint8_t* buffer) {
    block_device_t* dev = block_get_device(device_id);
    if (!dev) return -1;
    if (!dev->present || !dev->ops || !dev->ops->write_block) return -2;
    if (lba >= dev->block_count) return -3;

    return dev->ops->write_block(dev, lba, buffer);
}

int block_read_multi(uint8_t device_id, uint64_t lba, uint32_t count, uint8_t* buffer) {
    block_device_t* dev = block_get_device(device_id);
    if (!dev || !dev->present || !dev->ops) return -1;
    if (lba + count > dev->block_count) return -2;

    if (dev->ops->read_blocks) {
        return dev->ops->read_blocks(dev, lba, count, buffer);
    }

    if (!dev->ops->read_block) return -3;

    for (uint32_t i = 0; i < count; i++) {
        if (dev->ops->read_block(dev, lba + i, buffer + (i * dev->block_size)) != 0) {
            return -1;
        }
    }

    return 0;
}

int block_write_multi(uint8_t device_id, uint64_t lba, uint32_t count, const uint8_t* buffer) {
    block_device_t* dev = block_get_device(device_id);
    if (!dev || !dev->present || !dev->ops) return -1;
    if (lba + count > dev->block_count) return -2;

    if (dev->ops->write_blocks) {
        return dev->ops->write_blocks(dev, lba, count, buffer);
    }

    if (!dev->ops->write_block) return -3;

    for (uint32_t i = 0; i < count; i++) {
        if (dev->ops->write_block(dev, lba + i, buffer + (i * dev->block_size)) != 0) {
            return -1;
        }
    }

    return 0;
}

int block_flush(uint8_t device_id) {
    block_device_t* dev = block_get_device(device_id);
    if (!dev || !dev->present || !dev->ops) return -1;
    if (dev->ops->flush) return dev->ops->flush(dev);
    return 0;
}