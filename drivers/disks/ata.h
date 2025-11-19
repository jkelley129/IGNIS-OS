#ifndef ATA_H
#define ATA_H

#include "libc/stdint.h"
#include "error_handling/errno.h"

// ATA I/O Ports (Primary Bus)
#define ATA_PRIMARY_DATA       0x1F0
#define ATA_PRIMARY_ERROR      0x1F1
#define ATA_PRIMARY_SECCOUNT   0x1F2
#define ATA_PRIMARY_LBA_LOW    0x1F3
#define ATA_PRIMARY_LBA_MID    0x1F4
#define ATA_PRIMARY_LBA_HIGH   0x1F5
#define ATA_PRIMARY_DRIVE_HEAD 0x1F6
#define ATA_PRIMARY_STATUS     0x1F7
#define ATA_PRIMARY_COMMAND    0x1F7
#define ATA_PRIMARY_CONTROL    0x3F6

// ATA I/O Ports (Secondary Bus)
#define ATA_SECONDARY_DATA       0x170
#define ATA_SECONDARY_ERROR      0x171
#define ATA_SECONDARY_SECCOUNT   0x172
#define ATA_SECONDARY_LBA_LOW    0x173
#define ATA_SECONDARY_LBA_MID    0x174
#define ATA_SECONDARY_LBA_HIGH   0x175
#define ATA_SECONDARY_DRIVE_HEAD 0x176
#define ATA_SECONDARY_STATUS     0x177
#define ATA_SECONDARY_COMMAND    0x177
#define ATA_SECONDARY_CONTROL    0x376

// ATA Commands
#define ATA_CMD_READ_PIO        0x20
#define ATA_CMD_READ_PIO_EXT    0x24
#define ATA_CMD_WRITE_PIO       0x30
#define ATA_CMD_WRITE_PIO_EXT   0x34
#define ATA_CMD_CACHE_FLUSH     0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_IDENTIFY        0xEC

// ATA Status Register bits
#define ATA_SR_BSY  0x80  // Busy
#define ATA_SR_DRDY 0x40  // Drive ready
#define ATA_SR_DF   0x20  // Drive write fault
#define ATA_SR_DSC  0x10  // Drive seek complete
#define ATA_SR_DRQ  0x08  // Data request ready
#define ATA_SR_CORR 0x04  // Corrected data
#define ATA_SR_IDX  0x02  // Index
#define ATA_SR_ERR  0x01  // Error

// ATA Error Register bits
#define ATA_ER_BBK   0x80  // Bad block
#define ATA_ER_UNC   0x40  // Uncorrectable data
#define ATA_ER_MC    0x20  // Media changed
#define ATA_ER_IDNF  0x10  // ID not found
#define ATA_ER_MCR   0x08  // Media change request
#define ATA_ER_ABRT  0x04  // Command aborted
#define ATA_ER_TK0NF 0x02  // Track 0 not found
#define ATA_ER_AMNF  0x01  // No address mark

// Drive selection
#define ATA_MASTER 0xA0
#define ATA_SLAVE  0xB0

// Sector size
#define ATA_SECTOR_SIZE 512

// ATA device structure
typedef struct {
    uint16_t base;      // Base I/O port
    uint16_t ctrl;      // Control port
    uint8_t drive;      // Drive number (master/slave)
    uint8_t exists;     // Does the drive exist?
    uint32_t size;      // Size in sectors
} ata_device_t;

// Function declarations
// Only initialization is exposed - all I/O goes through block device abstraction
kerr_t ata_register();

#endif