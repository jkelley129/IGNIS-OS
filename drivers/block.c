#include "block.h"
#include "../io/vga.h"
#include "../libc/string.h"
#include "../error_handling/errno.h"

// Array of registered block devices
static block_device_t* block_devices[MAX_BLOCK_DEVICES];
static uint8_t device_count = 0;

void block_init(){
    for(int i = 0; i < MAX_BLOCK_DEVICES; i++){
        block_devices[i] = 0;
    }
    device_count = 0;
}

int block_register_device(block_device_t* device){
    if(device_count >= MAX_BLOCK_DEVICES){
        return -1;
    }

    device->id = device_count;
    block_devices[device_count] = device;
    device_count++;

    return device->id;
}

block_device_t* get_block_device(uint8_t id){
    if(id >= device_count){
        return 0;
    }
    return block_devices[id];
}

uint8_t block_get_device_count(){
    return device_count;
}

void block_list_devices(){
    vga_puts("\n=== Block Devices ===\n");

    if(device_count == 0){
        vga_puts("No block devices found");
        return;
    }

    for(uint8_t i = 0; i < device_count; i++){
        block_device_t* dev = block_devices[i];

        if(dev && dev->present){
            char num_str[32];

            vga_puts("Device ");
            uitoa(dev->id, num_str);
            vga_puts(num_str);
            vga_puts(": ");
            vga_puts(" (");

            switch(dev->type){
                case BLOCK_TYPE_ATA:
                    vga_puts("ATA");
                    break;
                case BLOCK_TYPE_AHCI:
                    vga_puts("AHCI");
                    break;
                case BLOCK_TYPE_NVME:
                    vga_puts("NVME");
                    break;
                case BLOCK_TYPE_RAMDISK:
                    vga_puts("RAM Disk");
                    break;
                default:
                    vga_puts("Unknown");
            }

            vga_puts(") - ");

            uint64_t size_mb = (dev->block_count * dev->block_size) / (1024 * 1024);
            uitoa(size_mb, num_str);
            vga_puts(num_str);
            vga_puts(" MB\n");
        }
    }
    vga_putc('\n');
}

// Generic I/O operations
int block_read(uint8_t device_id, uint64_t lba, uint8_t* buffer){
    block_device_t* dev = get_block_device(device_id);
    if(dev == 0) return -1;

    //Check if valid
    if(!dev || !dev->present || dev->ops || !dev->ops->read_block) return -1;

    //Check if lba is out of range
    if(lba >= dev->block_count) return -2;

    return dev->ops->read_block(dev, lba, buffer);
}

int block_write(uint8_t device_id, uint64_t lba, const uint8_t* buffer){
    block_device_t* dev = get_block_device(device_id);
    if(dev == 0) return -1;

    //Check if valid
    if(!dev || !dev->present || dev->ops || !dev->ops->write_block) return -2;

    //Check if lba is out of range
    if(lba >= dev->block_count) return -3;

    return dev->ops->write_block(dev, lba, buffer);
}

int block_read_multi(uint8_t device_id, uint64_t lba, uint32_t count, uint8_t* buffer){
    block_device_t* dev = block_get_device(device_id);
    if (!dev || !dev->present || !dev->ops) {
        return -1;
    }

    if (lba + count > dev->block_count) {
        return -2;  // Range exceeds device size
    }

    //Use optimized multi-read if available
    if(dev->ops->read_blocks){
        return dev->ops->read_blocks(dev,lba,count,buffer);
    }

    //Else, use single reads
    if(!dev->ops->read_block) return -3;

    for(uint32_t i = 0; i < count; i++){
        //Read each block
        // Increment sector choice by i, increment buffer position by i * block size to prevent overwriting
        if (dev->ops->read_block(dev, lba + i, buffer + (i * dev->block_size)) != 0) {
            return -1;
        }
    }

    return 0;
}

int block_write_multi(uint8_t device_id, uint64_t lba, uint32_t count, const uint8_t* buffer){
    block_device_t* dev = block_get_device(device_id);
    if (!dev || !dev->present || !dev->ops) {
        return -1;
    }

    if (lba + count > dev->block_count) {
        return -2;  // Range exceeds device size
    }

    //Use optimized multi-read if available
    if(dev->ops->write_blocks){
        return dev->ops->write_blocks(dev,lba,count,buffer);
    }

    //Else, use single reads
    if(!dev->ops->read_block) return -3;

    for(uint32_t i = 0; i < count; i++){
        //Read each block
        // Increment sector choice by i, increment buffer position by i * block size to prevent overwriting
        if (dev->ops->write_block(dev, lba + i, buffer + (i * dev->block_size)) != 0) {
            return -1;
        }
    }

    return 0;
}


int block_flush(uint8_t device_id){
    block_device_t* dev = block_get_device(device_id);

    if(!dev || !dev->present || !dev->ops) return -1;

    if(dev->ops->flush) return dev->ops->flush(dev);

    return 0;
}
