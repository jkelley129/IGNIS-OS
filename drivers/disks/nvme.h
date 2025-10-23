#ifndef NVME_H
#define NVME_H

#include "../../libc/stdint.h"
#include "../../error_handling/errno.h"

// NVMe Register Offsets
#define NVME_REG_CAP    0x00  // Controller Capabilities
#define NVME_REG_VS     0x08  // Version
#define NVME_REG_INTMS  0x0C  // Interrupt Mask Set
#define NVME_REG_INTMC  0x10  // Interrupt Mask Clear
#define NVME_REG_CC     0x14  // Controller Configuration
#define NVME_REG_CSTS   0x1C  // Controller Status
#define NVME_REG_AQA    0x24  // Admin Queue Attributes
#define NVME_REG_ASQ    0x28  // Admin Submission Queue Base Address
#define NVME_REG_ACQ    0x30  // Admin Completion Queue Base Address

// Controller Configuration Register bits
#define NVME_CC_ENABLE    (1 << 0)
#define NVME_CC_CSS_NVM   (0 << 4)  // NVM Command Set
#define NVME_CC_MPS_SHIFT 7
#define NVME_CC_AMS_RR    (0 << 11) // Round Robin arbitration
#define NVME_CC_SHN_NONE  (0 << 14) // No shutdown notification
#define NVME_CC_SHN_NORMAL (1 << 14) // Normal shutdown
#define NVME_CC_IOSQES    (6 << 16) // I/O Submission Queue Entry Size (2^6 = 64)
#define NVME_CC_IOCQES    (4 << 20) // I/O Completion Queue Entry Size (2^4 = 16)

// Controller Status Register bits
#define NVME_CSTS_RDY     (1 << 0)  // Ready
#define NVME_CSTS_CFS     (1 << 1)  // Controller Fatal Status
#define NVME_CSTS_SHST_NORMAL (0 << 2)  // Normal operation
#define NVME_CSTS_SHST_OCCURRING (1 << 2) // Shutdown occurring
#define NVME_CSTS_SHST_COMPLETE (2 << 2)  // Shutdown complete

// Queue sizes
#define NVME_ADMIN_QUEUE_SIZE 64
#define NVME_IO_QUEUE_SIZE    1024
#define NVME_MAX_NAMESPACES   16

// NVMe Admin Commands
#define NVME_ADMIN_DELETE_SQ   0x00
#define NVME_ADMIN_CREATE_SQ   0x01
#define NVME_ADMIN_DELETE_CQ   0x04
#define NVME_ADMIN_CREATE_CQ   0x05
#define NVME_ADMIN_IDENTIFY    0x06
#define NVME_ADMIN_SET_FEATURES 0x09
#define NVME_ADMIN_GET_FEATURES 0x0A

// NVMe I/O Commands
#define NVME_CMD_READ   0x02
#define NVME_CMD_WRITE  0x01

// Identify CNS values
#define NVME_IDENTIFY_NAMESPACE 0x00
#define NVME_IDENTIFY_CONTROLLER 0x01
#define NVME_IDENTIFY_NAMESPACE_LIST 0x02

// Status codes
#define NVME_SC_SUCCESS 0x00

// Submission Queue Entry (64 bytes)
typedef struct {
    uint32_t cdw0;      // Command Dword 0 (Opcode and flags)
    uint32_t nsid;      // Namespace Identifier
    uint64_t reserved;
    uint64_t mptr;      // Metadata Pointer
    uint64_t prp1;      // PRP Entry 1
    uint64_t prp2;      // PRP Entry 2
    uint32_t cdw10;
    uint32_t cdw11;
    uint32_t cdw12;
    uint32_t cdw13;
    uint32_t cdw14;
    uint32_t cdw15;
} __attribute__((packed)) nvme_sq_entry_t;

// Completion Queue Entry (16 bytes)
typedef struct {
    uint32_t dw0;       // Command specific
    uint32_t dw1;       // Reserved
    uint16_t sq_head;   // Submission Queue Head Pointer
    uint16_t sq_id;     // Submission Queue Identifier
    uint16_t cid;       // Command Identifier
    uint16_t status;    // Status Field
} __attribute__((packed)) nvme_cq_entry_t;

// Queue pair structure
typedef struct {
    nvme_sq_entry_t* sq;       // Submission queue
    nvme_cq_entry_t* cq;       // Completion queue
    uint64_t sq_phys;           // Physical address of SQ
    uint64_t cq_phys;           // Physical address of CQ
    uint16_t sq_tail;           // SQ tail pointer
    uint16_t cq_head;           // CQ head pointer
    uint16_t sq_size;           // SQ size
    uint16_t cq_size;           // CQ size
    uint8_t cq_phase;           // CQ phase bit
} nvme_queue_pair_t;

// NVMe Controller Identify structure (relevant fields)
typedef struct {
    uint16_t vid;               // PCI Vendor ID
    uint16_t ssvid;             // PCI Subsystem Vendor ID
    char sn[20];                // Serial Number
    char mn[40];                // Model Number
    char fr[8];                 // Firmware Revision
    uint8_t rab;                // Recommended Arbitration Burst
    uint8_t ieee[3];            // IEEE OUI Identifier
    uint8_t cmic;               // Controller Multi-Path I/O
    uint8_t mdts;               // Maximum Data Transfer Size
    uint16_t cntlid;            // Controller ID
    uint32_t ver;               // Version
    uint8_t reserved1[172];
    uint16_t oacs;              // Optional Admin Command Support
    uint8_t acl;                // Abort Command Limit
    uint8_t aerl;               // Async Event Request Limit
    uint8_t frmw;               // Firmware Updates
    uint8_t lpa;                // Log Page Attributes
    uint8_t elpe;               // Error Log Page Entries
    uint8_t npss;               // Number of Power States Support
    uint8_t reserved2[248];
    uint8_t sqes;               // Submission Queue Entry Size
    uint8_t cqes;               // Completion Queue Entry Size
    uint8_t reserved3[2];
    uint32_t nn;                // Number of Namespaces
    uint8_t reserved4[1344];
} __attribute__((packed)) nvme_identify_controller_t;

// NVMe Namespace Identify structure (relevant fields)
typedef struct {
    uint64_t nsze;              // Namespace Size
    uint64_t ncap;              // Namespace Capacity
    uint64_t nuse;              // Namespace Utilization
    uint8_t nsfeat;             // Namespace Features
    uint8_t nlbaf;              // Number of LBA Formats
    uint8_t flbas;              // Formatted LBA Size
    uint8_t mc;                 // Metadata Capabilities
    uint8_t dpc;                // End-to-end Data Protection Capabilities
    uint8_t dps;                // End-to-end Data Protection Type Settings
    uint8_t reserved1[98];
    struct {
        uint16_t ms;            // Metadata Size
        uint8_t lbads;          // LBA Data Size (2^n)
        uint8_t rp;             // Relative Performance
    } lbaf[16];                 // LBA Format Support
    uint8_t reserved2[192];
    uint8_t vendor_specific[3712];
} __attribute__((packed)) nvme_identify_namespace_t;

// NVMe Controller structure
typedef struct {
    volatile uint8_t* bar0;     // Base Address Register 0 (memory mapped)
    nvme_queue_pair_t admin_queue;
    nvme_queue_pair_t io_queue;
    uint32_t num_namespaces;
    uint32_t max_transfer_size;
    uint16_t command_id;
} nvme_controller_t;

// Function declarations
kerr_t nvme_init();
int nvme_identify_controller(nvme_controller_t* ctrl, nvme_identify_controller_t* id);
int nvme_identify_namespace(nvme_controller_t* ctrl, uint32_t nsid, nvme_identify_namespace_t* id);

kerr_t nvme_register();

#endif