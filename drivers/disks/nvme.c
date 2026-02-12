#include "nvme.h"
#include "block.h"
#include "driver.h"
#include "console/console.h"
#include "io/serial.h"
#include "io/ports.h"
#include "libc/string.h"
#include "mm/memory.h"
#include "mm/allocators/kmalloc.h"
#include "error_handling/errno.h"
#include "interrupts/idt.h"
#include "mm/memory_layout.h"
#include "mm/vmm.h"

// PCI Configuration Space
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

// PCI Header registers
#define PCI_VENDOR_ID      0x00
#define PCI_DEVICE_ID      0x02
#define PCI_COMMAND        0x04
#define PCI_BAR0           0x10
#define PCI_BAR1           0x14

// PCI Command register bits
#define PCI_COMMAND_IO          0x01
#define PCI_COMMAND_MEMORY      0x02
#define PCI_COMMAND_MASTER      0x04
#define PCI_COMMAND_INTDISABLE  0x400

static nvme_controller_t nvme_ctrl;
static block_device_t nvme_block_devices[NVME_MAX_NAMESPACES];

static driver_t nvme_driver = {
    .name = "NVMe",
    .type = DRIVER_TYPE_BLOCK,
    .version = 1,
    .priority = 40,  // Initialize after block layer (priority 30)
    .init = nvme_init,
    .cleanup = NULL,
    .depends_on = "Block Layer",  // Depends on block device layer
    .driver_data = NULL
};

//Helper function to read NVMe register
static inline uint32_t nvme_read32(nvme_controller_t* ctrl, uint32_t offset){
    return *((volatile uint32_t*)(ctrl->bar0 + offset));
}

//Helper function to write NVMe register
static inline void nvme_write32(nvme_controller_t* ctrl, uint32_t offset, uint32_t value){
    *((volatile uint32_t*)(ctrl->bar0 + offset)) = value;
}

//Helper function to read 64-bit NVMe register
static inline uint64_t nvme_read64(nvme_controller_t* ctrl, uint32_t offset){
    return *((volatile uint64_t*)(ctrl->bar0 + offset));
}

//Helper function to write 64-bit NVMe register
static inline void nvme_write64(nvme_controller_t* ctrl, uint32_t offset, uint64_t value){
    *((volatile uint64_t*)(ctrl->bar0 + offset)) = value;
}

//Helper function to get PCI address
static inline uint32_t pci_get_address(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
    return (uint32_t)((bus << 16) | (slot << 11) | (func << 8) | (offset & 0xFC) | 0x80000000);
}

//PCI configuration space access
static uint32_t pci_read_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset){
    uint32_t address = pci_get_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

static void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value){
    uint32_t address = pci_get_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA, value);
}


//Find NVMe controller on PCI bus
static int nvme_find_controller(uint8_t* bus, uint8_t* slot, uint8_t* func){
    serial_debug_puts("[NVME] Starting PCI scan...\n");
    serial_debug_puts("[NVME] Testing PCI config space access...\n");

    // Test a safe read first (PCI device 0:0:0 always exists)
    uint32_t test = pci_read_config(0, 0, 0, 0x00);
    serial_debug_puts("[NVME] Test read: 0x");
    serial_puthex(COM1, test, 8);
    serial_debug_puts("\n");

    serial_debug_puts("[NVME] Scanning PCI bus for NVMe controller...\n");

    for (uint16_t b = 0; b < 256; b++){
        for(uint8_t s = 0; s < 32; s++){
            // Try to read vendor ID first - if it fails, skip
            uint32_t vendor_device;

            // Add error checking for PCI reads
            idt_disable_interrupts();

            vendor_device = pci_read_config(b, s, 0, 0x00);

            idt_enable_interrupts();

            uint16_t vendor = vendor_device & 0xFFFF;

            // Skip if no device present
            if(vendor == 0xFFFF || vendor == 0x0000) continue;

            uint16_t device = (vendor_device >> 16) & 0xFFFF;

            // Read class code register
            idt_disable_interrupts();
            uint32_t class_reg = pci_read_config(b, s, 0, 0x08);
            idt_enable_interrupts();

            uint8_t class_code = (class_reg >> 24) & 0xFF;
            uint8_t subclass = (class_reg >> 16) & 0xFF;
            uint8_t prog_if = (class_reg >> 8) & 0xFF;

            serial_debug_puts("[NVME] Device at ");
            serial_puthex(COM1, b, 2);
            serial_debug_puts(":");
            serial_puthex(COM1, s, 2);
            serial_debug_puts(" - Vendor: 0x");
            serial_puthex(COM1, vendor, 4);
            serial_debug_puts(" Device: 0x");
            serial_puthex(COM1, device, 4);
            serial_debug_puts(" Class: 0x");
            serial_puthex(COM1, class_code, 2);
            serial_debug_puts("/0x");
            serial_puthex(COM1, subclass, 2);
            serial_debug_puts("/0x");
            serial_puthex(COM1, prog_if, 2);
            serial_debug_puts("\n");

            // Check for NVMe Express controller
            // Class 0x01 = Mass Storage Controller
            // Subclass 0x08 = Non-Volatile Memory controller
            // ProgIF 0x02 = NVM Express
            if(class_code == 0x01 && subclass == 0x08 && prog_if == 0x02){
                serial_debug_puts("[NVME] Found NVMe controller!\n");
                *bus = b;
                *slot = s;
                *func = 0;
                return 1;
            }
        }
    }

    serial_debug_puts("[NVME] No NVMe controller found\n");
    return 0;
}

static kerr_t nvme_init_queue_pair(nvme_queue_pair_t* qp, uint16_t sq_size, uint16_t cq_size){
    // Calculate sizes
    size_t sq_bytes = sq_size * sizeof(nvme_sq_entry_t);
    size_t cq_bytes = cq_size * sizeof(nvme_cq_entry_t);

    // Calculate pages needed (round up)
    size_t sq_pages = (sq_bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t cq_pages = (cq_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    // Allocate page-aligned submission queue
    qp->sq = (nvme_sq_entry_t*)kmalloc_pages(sq_pages);
    if(!qp->sq) return E_NOMEM;
    memset(qp->sq, 0, sq_bytes);
    qp->sq_phys = VIRT_TO_PHYS((uint64_t)qp->sq);

    // Allocate page-aligned completion queue
    qp->cq = (nvme_cq_entry_t*)kmalloc_pages(cq_pages);
    if(!qp->cq) {
        kfree_pages(qp->sq, sq_pages);
        return E_NOMEM;
    }
    memset(qp->cq, 0, cq_bytes);
    qp->cq_phys = VIRT_TO_PHYS((uint64_t)qp->cq);

    qp->sq_size = sq_size;
    qp->cq_size = cq_size;
    qp->sq_tail = 0;
    qp->cq_head = 0;
    qp->cq_phase = 1;

    // Add debug output to verify alignment
    serial_debug_puts("[NVME] Queue pair allocated:\n");
    serial_debug_puts("  SQ virt: 0x");
    serial_puthex(COM1, (uint64_t)qp->sq, 16);
    serial_debug_puts(" phys: 0x");
    serial_puthex(COM1, qp->sq_phys, 16);
    serial_debug_puts("\n  CQ virt: 0x");
    serial_puthex(COM1, (uint64_t)qp->cq, 16);
    serial_debug_puts(" phys: 0x");
    serial_puthex(COM1, qp->cq_phys, 16);
    serial_debug_puts("\n");

    // Verify alignment
    if ((qp->sq_phys & (PAGE_SIZE - 1)) != 0) {
        serial_debug_puts("[NVME] ERROR: SQ not page-aligned!\n");
        return E_INVALID;
    }
    if ((qp->cq_phys & (PAGE_SIZE - 1)) != 0) {
        serial_debug_puts("[NVME] ERROR: CQ not page-aligned!\n");
        return E_INVALID;
    }

    return E_OK;
}

static void nvme_submit_command(nvme_controller_t* ctrl, nvme_queue_pair_t* qp,
                                nvme_sq_entry_t* cmd, uint8_t is_admin){
    //Copy command to submission queue
    qp->sq[qp->sq_tail] = *cmd;

    //Update tail pointer
    qp->sq_tail = (qp->sq_tail + 1) % qp->sq_size;

    //Ring doorbell
    if(is_admin){
        nvme_write32(ctrl, 0x1000, qp->sq_tail);
    }else{
        nvme_write32(ctrl, 0x1000 + (2 * 4), qp->sq_tail);
    }
}

//Wait for command completion
static kerr_t nvme_wait_completion(nvme_controller_t* ctrl, nvme_queue_pair_t* qp,
                                   uint16_t cid, uint8_t is_admin){
    uint32_t timeout = 5e6;//5-second timeout


    while(timeout--){
        nvme_cq_entry_t* cqe = &qp->cq[qp->cq_head];
        uint8_t phase = (cqe->status >> 0) & 1;

        if (phase == qp->cq_phase) {
            // Check if this is our command
            if (cqe->cid == cid) {
                uint16_t status = (cqe->status >> 1) & 0x7FF;

                // Update head pointer
                qp->cq_head = (qp->cq_head + 1) % qp->cq_size;
                if (qp->cq_head == 0) {
                    qp->cq_phase = !qp->cq_phase;
                }

                // Ring doorbell
                if (is_admin) {
                    nvme_write32(ctrl, 0x1000 + 4, qp->cq_head);  // Admin CQ doorbell
                } else {
                    nvme_write32(ctrl, 0x1000 + (2 * 4) + 4, qp->cq_head);  // I/O CQ doorbell
                }

                return (status == NVME_SC_SUCCESS) ? E_OK : E_HARDWARE;
            }
        }
        //For small delay
        for (volatile uint8_t i = 0; i < 100; i++);
    }
    return E_TIMEOUT;
}

//Create I/O completion queue
static kerr_t nvme_create_io_cq(nvme_controller_t* ctrl) {
    nvme_sq_entry_t* cmd = kmalloc(sizeof(nvme_sq_entry_t*));

    cmd->cdw0 = NVME_ADMIN_CREATE_CQ;
    cmd->cdw0 |= (ctrl->command_id++ << 16);
    cmd->prp1 = ctrl->io_queue.cq_phys;
    cmd->cdw10 = ((ctrl->io_queue.cq_size - 1) << 16) | 1;  // Queue size and ID
    cmd->cdw11 = 0x1;  // Physically contiguous

    nvme_submit_command(ctrl, &ctrl->admin_queue, cmd, 1);
    return nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd->cdw0 >> 16) & 0xFFFF, 1);
}


//Create I/O submission queue
static kerr_t nvme_create_io_sq(nvme_controller_t* ctrl) {
    nvme_sq_entry_t* cmd = kmalloc(sizeof(nvme_sq_entry_t*));

    cmd->cdw0 = NVME_ADMIN_CREATE_SQ;
    cmd->cdw0 |= (ctrl->command_id++ << 16);
    cmd->prp1 = ctrl->io_queue.sq_phys;
    cmd->cdw10 = ((ctrl->io_queue.sq_size - 1) << 16) | 1;
    cmd->cdw11 = (1 << 16) | 0x1;

    nvme_submit_command(ctrl, &ctrl->admin_queue, cmd, 1);
    return nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd->cdw0 >> 16) & 0xFFFF,1);
}

//Identify controller
int nvme_identify_controller(nvme_controller_t* ctrl, nvme_identify_controller_t* id) {
    serial_debug_puts("[NVME] Allocating identify buffer...\n");

    void* buffer = kmalloc(4096);
    if(!buffer) {
        serial_debug_puts("[NVME] Failed to allocate buffer\n");
        return E_NOMEM;
    }
    memset(buffer, 0, 4096);

    serial_debug_puts("[NVME] Buffer allocated at: 0x");
    serial_puthex(COM1, (uint64_t)buffer, 16);
    serial_debug_puts("\n");

    // FIX: Convert to physical address for DMA
    uint64_t buffer_phys = VIRT_TO_PHYS((uint64_t)buffer);
    serial_debug_puts("[NVME] Buffer physical: 0x");
    serial_puthex(COM1, buffer_phys, 16);
    serial_debug_puts("\n");

    serial_debug_puts("[NVME] Building identify command...\n");

    serial_debug_puts("nvme_sq_entry_t cmd = {0};");
    nvme_sq_entry_t* cmd = kmalloc(sizeof(nvme_sq_entry_t));
    if (!cmd) {
        serial_debug_puts("[NVME] Failed to allocate command buffer\n");
        return E_NOMEM;
    }
    memset(cmd, 0, sizeof(*cmd));

    if (!ctrl || !ctrl->admin_queue.sq) {
        serial_debug_puts("[NVME] Admin queue not initialized!\n");
        kfree(cmd);
        return E_HARDWARE;
    }

    cmd->cdw0 = NVME_ADMIN_IDENTIFY;
    cmd->cdw0 |= (ctrl->command_id++ << 16);
    cmd->nsid = 0;
    cmd->prp1 = buffer_phys;
    cmd->cdw10 = NVME_IDENTIFY_CONTROLLER;

    serial_debug_puts("[NVME] Command ID: ");
    char num_str[16];
    uitoa((cmd->cdw0 >> 16) & 0xFFFF, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("\n");

    serial_debug_puts("[NVME] Submitting command...\n");
    nvme_submit_command(ctrl, &ctrl->admin_queue, cmd, 1);

    serial_debug_puts("[NVME] Waiting for completion...\n");
    kerr_t err = nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd->cdw0 >> 16) & 0xFFFF, 1);
    kfree(cmd);
    serial_debug_puts("[NVME] Completion status: ");
    uitoa(err, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("\n");

    if (err == E_OK){
        serial_debug_puts("[NVME] Copying data...\n");
        //Copy data
        uint8_t* src = (uint8_t*)buffer;
        uint8_t* dst = (uint8_t*)id;
        for(size_t i = 0; i < sizeof(nvme_identify_controller_t); i++){
            dst[i] = src[i];
        }
        serial_debug_puts("[NVME] Data copied\n");
    }

    kfree(buffer);
    serial_debug_puts("[NVME] Identify controller complete\n");
    return err;
}

//Identify namespace
int nvme_identify_namespace(nvme_controller_t* ctrl, uint32_t nsid, nvme_identify_namespace_t* id) {
    serial_debug_puts("[NVME] Identifying namespace ");
    char num_str[16];
    uitoa(nsid, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("...\n");

    serial_debug_puts("[NVME] Allocating identify buffer...\n");
    void* buffer = kmalloc(4096);
    serial_debug_puts("[NVME] Buffer allocated at: 0x");
    serial_puthex(COM1, (uint64_t)buffer, 16);
    serial_debug_putc('\n');
    if(!buffer) {
        serial_debug_puts("[NVME] Failed to allocate buffer\n");
        return E_NOMEM;
    }
    memset(buffer, 0, 4096);

    // FIX: Convert to physical address for DMA
    uint64_t buffer_phys = VIRT_TO_PHYS((uint64_t)buffer);

    nvme_sq_entry_t* cmd = kmalloc(sizeof(nvme_sq_entry_t*));
    cmd->cdw0 = NVME_ADMIN_IDENTIFY;
    cmd->cdw0 |= (ctrl->command_id++ << 16);
    cmd->nsid = nsid;
    cmd->prp1 = buffer_phys;  // FIX: Use physical address!
    cmd->cdw10 = NVME_IDENTIFY_NAMESPACE;

    serial_debug_puts("[NVME](Identify Namespace) Submitting command");
    nvme_submit_command(ctrl, &ctrl->admin_queue, cmd, 1);
    kerr_t err = nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd->cdw0 >> 16) & 0xFFFF, 1);

    if (err == E_OK) {
        //Copy data
        uint8_t* src = (uint8_t*)buffer;
        uint8_t* dst = (uint8_t*)id;
        for(size_t i = 0; i < sizeof(nvme_identify_namespace_t); i++){
            dst[i] = src[i];
        }
    }

    kfree(buffer);
    return err;
}

// Block device operations
static kerr_t nvme_read_block_op(block_device_t* dev, uint64_t lba, uint8_t* buffer) {
    nvme_controller_t* ctrl = (nvme_controller_t*)dev->driver_data;

    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = NVME_CMD_READ;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.nsid = (uint32_t)(dev->id + 1);  // Namespace ID (1-based)
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = 0;  // Read 1 block (0-based count)

    nvme_submit_command(ctrl, &ctrl->io_queue, &cmd, 0);
    return nvme_wait_completion(ctrl, &ctrl->io_queue, (cmd.cdw0 >> 16) & 0xFFFF, 0);
}

static kerr_t nvme_write_block_op(block_device_t* dev, uint64_t lba, const uint8_t* buffer) {
    nvme_controller_t* ctrl = (nvme_controller_t*)dev->driver_data;

    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = NVME_CMD_WRITE;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.nsid = (uint32_t)(dev->id + 1);  // Namespace ID (1-based)
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = (uint32_t)lba;
    cmd.cdw11 = (uint32_t)(lba >> 32);
    cmd.cdw12 = 0;  // Write 1 block (0-based count)

    nvme_submit_command(ctrl, &ctrl->io_queue, &cmd, 0);
    return nvme_wait_completion(ctrl, &ctrl->io_queue, (cmd.cdw0 >> 16) & 0xFFFF, 0);
}

static kerr_t nvme_flush_op(block_device_t* dev) {
    //Don't have to implement, NVMe manages caches, but we can later
    return E_OK;
}

static const block_device_ops_t nvme_ops = {
        .read_block = nvme_read_block_op,
        .write_block = nvme_write_block_op,
        .read_blocks = 0,
        .write_blocks = 0,
        .flush = nvme_flush_op
};

kerr_t nvme_init() {
    uint8_t bus, slot, func;

    //Find the NVMe controller
    if(!nvme_find_controller(&bus,&slot,&func)) {
        return E_NOTFOUND;
    }

    console_puts("   Found NVMe controller at ");
    char num_str[32];
    uitoa(bus, num_str);
    console_puts(num_str);
    console_putc(':');
    uitoa(slot, num_str);
    console_puts(num_str);
    console_putc('\n');

    serial_debug_puts("[NVME] Found NVMe controller at PCI ");
    serial_puthex(COM1, bus, 2);
    serial_debug_puts(":");
    serial_puthex(COM1, slot, 2);
    serial_debug_puts("\n");

    //Enable PCI bus mastering and memory space
    uint16_t command = pci_read_config(bus, slot, func, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_write_config(bus, slot, func, PCI_COMMAND, command);

    serial_debug_puts("[NVME] Enabled PCI bus mastering and memory space\n");

    //Get BAR0
    uint32_t bar0_low = pci_read_config(bus, slot, func, PCI_BAR0);
    uint32_t bar0_high = pci_read_config(bus, slot, func, PCI_BAR1);
    uint64_t bar0_phys = ((uint64_t)bar0_high << 32) | (bar0_low & 0xFFFFFFF0);

    serial_debug_puts("[NVME] BAR0 physical: 0x");
    serial_puthex(COM1, bar0_phys, 16);
    serial_debug_puts("\n");

    // Calculate virtual address in direct-map region
    uint64_t bar0_virt = (uint64_t)PHYS_TO_VIRT(bar0_phys);

    serial_debug_puts("[NVME] BAR0 virtual (calculated): 0x");
    serial_puthex(COM1, bar0_virt, 16);
    serial_debug_puts("\n");

    // Map the BAR to virtual memory (map 64KB to be safe)
    // NVMe register space is typically 4KB-8KB
    size_t bar_size = 64 * 1024;  // 64KB
    for (size_t offset = 0; offset < bar_size; offset += PAGE_SIZE) {
        uint64_t virt_page = PAGE_ALIGN_DOWN(bar0_virt + offset);
        uint64_t phys_page = PAGE_ALIGN_DOWN(bar0_phys + offset);

        // Check if already mapped
        if (!vmm_is_mapped(virt_page)) {
            kerr_t err = vmm_map_page(
                virt_page,
                phys_page,
                PAGE_PRESENT | PAGE_WRITE | PAGE_CACHE_DISABLE
            );

            if (err != E_OK) {
                serial_debug_puts("[NVME] Failed to map page at 0x");
                serial_puthex(COM1, virt_page, 16);
                serial_debug_puts("\n");
                return E_HARDWARE;
            }
        }
    }

    serial_debug_puts("[NVME] BAR0 mapping complete\n");

    nvme_ctrl.bar0 = (volatile uint8_t*)bar0_virt;

    // Test read to verify mapping works
    serial_debug_puts("[NVME] Testing BAR0 access...\n");
    uint32_t cap_low = nvme_read32(&nvme_ctrl, NVME_REG_CAP);
    serial_debug_puts("[NVME] CAP register (low): 0x");
    serial_puthex(COM1, cap_low, 8);
    serial_debug_puts("\n");

    //Disable controller
    serial_debug_puts("[NVME] Disabling controller...\n");
    uint32_t cc = nvme_read32(&nvme_ctrl, NVME_REG_CC);
    serial_debug_puts("[NVME] Current CC: 0x");
    serial_puthex(COM1, cc, 8);
    serial_debug_puts("\n");

    cc &= ~NVME_CC_ENABLE;
    nvme_write32(&nvme_ctrl, NVME_REG_CC, cc);

    //Wait for controller to be disabled
    uint32_t timeout = 1000000;
    uint32_t csts;
    while (timeout--) {
        csts = nvme_read32(&nvme_ctrl, NVME_REG_CSTS);
        if (!(csts & NVME_CSTS_RDY)) {
            serial_debug_puts("[NVME] Controller disabled (CSTS: 0x");
            serial_puthex(COM1, csts, 8);
            serial_debug_puts(")\n");
            break;
        }

        // Small delay
        for (volatile int i = 0; i < 10; i++);
    }

    if (nvme_read32(&nvme_ctrl, NVME_REG_CSTS) & NVME_CSTS_RDY) {
        serial_debug_puts("[NVME] Timeout waiting for controller disable\n");
        return E_TIMEOUT;
    }

    // Initialize admin queues
    serial_debug_puts("[NVME] Initializing admin queues...\n");
    if (nvme_init_queue_pair(&nvme_ctrl.admin_queue, NVME_ADMIN_QUEUE_SIZE,
                             NVME_ADMIN_QUEUE_SIZE) != E_OK) {
        serial_debug_puts("[NVME] Failed to allocate admin queues\n");
        return E_HARDWARE;
    }

    serial_debug_puts("[NVME] Admin queues allocated\n");
    serial_debug_puts("[NVME] ASQ phys: 0x");
    serial_puthex(COM1, nvme_ctrl.admin_queue.sq_phys, 16);
    serial_debug_puts("\n[NVME] ACQ phys: 0x");
    serial_puthex(COM1, nvme_ctrl.admin_queue.cq_phys, 16);
    serial_debug_puts("\n");

    // Set admin queue addresses
    nvme_write64(&nvme_ctrl, NVME_REG_ASQ, nvme_ctrl.admin_queue.sq_phys);
    nvme_write64(&nvme_ctrl, NVME_REG_ACQ, nvme_ctrl.admin_queue.cq_phys);

    // Set admin queue sizes
    uint32_t aqa = ((NVME_ADMIN_QUEUE_SIZE - 1) << 16) | (NVME_ADMIN_QUEUE_SIZE - 1);
    nvme_write32(&nvme_ctrl, NVME_REG_AQA, aqa);

    serial_debug_puts("[NVME] Admin queue attributes set (AQA: 0x");
    serial_puthex(COM1, aqa, 8);
    serial_debug_puts(")\n");

    // Read CAP register to determine supported features
    uint64_t cap = nvme_read64(&nvme_ctrl, NVME_REG_CAP);
    serial_debug_puts("[NVME] CAP: 0x");
    serial_puthex(COM1, cap, 16);
    serial_debug_puts("\n");

    // Calculate Memory Page Size (MPS) - typically 0 for 4KB pages
    uint8_t mps_min = (cap >> 48) & 0xF;  // MPSMIN field
    serial_debug_puts("[NVME] MPS min: ");
    uitoa(mps_min, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("\n");

    // Configure and enable controller
    cc = NVME_CC_ENABLE |
         NVME_CC_CSS_NVM |
         (0 << 7) |  // MPS = 0 (4KB pages)
         NVME_CC_AMS_RR |
         NVME_CC_SHN_NONE |
         NVME_CC_IOSQES |
         NVME_CC_IOCQES;

    serial_debug_puts("[NVME] Enabling controller with CC: 0x");
    serial_puthex(COM1, cc, 8);
    serial_debug_puts("\n");

    nvme_write32(&nvme_ctrl, NVME_REG_CC, cc);

    // Wait for controller to be ready
    serial_debug_puts("[NVME] Waiting for controller ready...\n");
    timeout = 5000000;  // 5 second timeout
    while (timeout--) {
        csts = nvme_read32(&nvme_ctrl, NVME_REG_CSTS);

        // Check for fatal error
        if (csts & NVME_CSTS_CFS) {
            serial_debug_puts("[NVME] Controller fatal status! CSTS: 0x");
            serial_puthex(COM1, csts, 8);
            serial_debug_puts("\n");
            return E_HARDWARE;
        }

        if (csts & NVME_CSTS_RDY) {
            serial_debug_puts("[NVME] Controller ready! CSTS: 0x");
            serial_puthex(COM1, csts, 8);
            serial_debug_puts("\n");
            break;
        }

        // Debug every 500k iterations
        if (timeout % 500000 == 0) {
            serial_debug_puts("[NVME] Still waiting... CSTS: 0x");
            serial_puthex(COM1, csts, 8);
            serial_debug_puts("\n");
        }

        // Small delay
        for (volatile int i = 0; i < 10; i++);
    }

    if (!(nvme_read32(&nvme_ctrl, NVME_REG_CSTS) & NVME_CSTS_RDY)) {
        serial_debug_puts("[NVME] Timeout waiting for ready! Final CSTS: 0x");
        serial_puthex(COM1, nvme_read32(&nvme_ctrl, NVME_REG_CSTS), 8);
        serial_debug_puts("\n");
        return E_HARDWARE;
    }

    nvme_ctrl.command_id = 0;

    // Identify controller
    serial_debug_puts("[NVME] Identifying controller...\n");
    nvme_identify_controller_t ctrl_id;
    if (nvme_identify_controller(&nvme_ctrl, &ctrl_id) != E_OK) {
        serial_debug_puts("[NVME] Failed to identify controller\n");
        return E_HARDWARE;
    }

    nvme_ctrl.num_namespaces = ctrl_id.nn;
    serial_debug_puts("[NVME] Number of namespaces: ");
    uitoa(ctrl_id.nn, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("\n");

    // Initialize I/O queues
    serial_debug_puts("[NVME] Initializing I/O queues...\n");
    if (nvme_init_queue_pair(&nvme_ctrl.io_queue, NVME_IO_QUEUE_SIZE,
                             NVME_IO_QUEUE_SIZE) != E_OK) {
        serial_debug_puts("[NVME] Failed to allocate I/O queues\n");
        return E_HARDWARE;
    }

    serial_debug_puts("[NVME] I/O queues allocated\n");
    serial_debug_puts("[NVME] IOSQ phys: 0x");
    serial_puthex(COM1, nvme_ctrl.io_queue.sq_phys, 16);
    serial_debug_puts("\n[NVME] IOCQ phys: 0x");
    serial_puthex(COM1, nvme_ctrl.io_queue.cq_phys, 16);
    serial_debug_puts("\n");

    serial_debug_puts("[NVME] Creating I/O completion queue...\n");
    if (nvme_create_io_cq(&nvme_ctrl) != E_OK) {
        serial_debug_puts("[NVME] Failed to create I/O CQ\n");
        return E_HARDWARE;
    }

    serial_debug_puts("[NVME] Creating I/O submission queue...\n");
    if (nvme_create_io_sq(&nvme_ctrl) != E_OK) {
        serial_debug_puts("[NVME] Failed to create I/O SQ\n");
        return E_HARDWARE;
    }

    serial_debug_puts("[NVME] I/O queues created successfully\n");

    // Enumerate namespaces
    serial_debug_puts("[NVME] Enumerating namespaces...\n");
    for (uint32_t i = 0; i < nvme_ctrl.num_namespaces && i < NVME_MAX_NAMESPACES; i++) {
        serial_debug_puts("[NVME] Identifying namespace ");
        uitoa(i + 1, num_str);
        serial_debug_puts(num_str);
        serial_debug_puts("...\n");

        nvme_identify_namespace_t ns_id;
        if (nvme_identify_namespace(&nvme_ctrl, i + 1, &ns_id) == E_OK) {
            if (ns_id.nsze > 0) {
                block_device_t* dev = &nvme_block_devices[i];
                dev->type = BLOCK_TYPE_NVME;
                dev->block_count = ns_id.nsze;
                dev->block_size = 1 << ns_id.lbaf[ns_id.flbas & 0xF].lbads;
                dev->present = 1;
                dev->driver_data = &nvme_ctrl;
                dev->ops = &nvme_ops;

                // Create device label
                strcpy(dev->label, "NVME");
                uitoa(i, num_str);
                strcat(dev->label, num_str);

                block_register_device(dev);

                console_puts("  ");
                console_puts(dev->label);
                console_puts(": Found (");
                uint64_t size_mb = (dev->block_count * dev->block_size) / (1024 * 1024);
                uitoa(size_mb, num_str);
                console_puts(num_str);
                console_puts(" MB)\n");

                serial_debug_puts("[NVME] Registered namespace ");
                uitoa(i + 1, num_str);
                serial_debug_puts(num_str);
                serial_debug_puts(" - ");
                uitoa(size_mb, num_str);
                serial_debug_puts(num_str);
                serial_debug_puts(" MB\n");
            }
        }
    }

    serial_debug_puts("[NVME] Initialization complete\n");
    return E_OK;
}

kerr_t nvme_register() {
    driver_register(&nvme_driver);
    return E_OK;
}