#ifndef BLOCK_H
#define BLOCK_H

#include "../libc/stdint.h"
#include "../error_handling/errno.h"
#include "../libc/stdint.h"

#define BLOCK_SIZE 512
#define MAX_BLOCK_DEVICES 8

// Block device types
typedef enum {
    BLOCK_TYPE_NONE = 0,
    BLOCK_TYPE_ATA,
    BLOCK_TYPE_AHCI,
    BLOCK_TYPE_NVME,
    BLOCK_TYPE_RAMDISK
} block_device_type_t;

// Forward declaration
struct block_device;

// Block device operations structure (function pointers)
typedef struct {
    int (*read_block)(struct block_device* dev, uint64_t lba, uint8_t* buffer);
    int (*write_block)(struct block_device* dev, uint64_t lba, const uint8_t* buffer);
    int (*read_blocks)(struct block_device* dev, uint64_t lba, uint32_t count, uint8_t* buffer);
    int (*write_blocks)(struct block_device* dev, uint64_t lba, uint32_t count, const uint8_t* buffer);
    int (*flush)(struct block_device* dev);
} block_device_ops_t;

// Block device structure
typedef struct block_device {
    uint8_t id;                      // Device ID
    block_device_type_t type;        // Device type
    uint64_t block_count;            // Total number of blocks
    uint16_t block_size;             // Block size in bytes (usually 512)
    uint8_t present;                 // Is device present?
    char label[32];                  // Device label (e.g., "ATA0", "NVME0")
    void* driver_data;               // Driver-specific data
    const block_device_ops_t* ops;   // Operations for this device
} block_device_t;

// Block device manager functions
void block_init();
int block_register_device(block_device_t* device);
block_device_t* block_get_device(uint8_t id);
uint8_t block_get_device_count();
void block_list_devices();

// Generic block I/O operations (these dispatch to the appropriate driver)
int block_read(uint8_t device_id, uint64_t lba, uint8_t* buffer);
int block_write(uint8_t device_id, uint64_t lba, const uint8_t* buffer);
int block_read_multi(uint8_t device_id, uint64_t lba, uint32_t count, uint8_t* buffer);
int block_write_multi(uint8_t device_id, uint64_t lba, uint32_t count, const uint8_t* buffer);
int block_flush(uint8_t device_id);

#endif