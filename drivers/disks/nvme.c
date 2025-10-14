#include "nvme.h"
#include "../block.h"
#include "../../console/console.h"
#include "../../io/ports.h"
#include "../../libc/string.h"
#include "../../mm/memory.h"
#include "../../error_handling/errno.h"

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
    uint32_t address = pci_get_address(bus,slot,func,offset);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA + (offset & 3));
}

static void pci_write_config(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint32_t value){
    uint32_t address = pci_get_address(bus, slot, func, offset);
    outl(PCI_CONFIG_ADDRESS, address);
    outl(PCI_CONFIG_DATA + (offset & 3), value);
}


//Find NVMe controller on PCI bus
static int nvme_find_controller(uint8_t* bus, uint8_t* slot, uint8_t* func){
    for (uint16_t b = 0; b < 256; b++){
        for(uint8_t s = 0; s < 32; s++){
            uint16_t vendor = pci_read_config(b, s, 0, PCI_VENDOR_ID);
            if(vendor == 0xFFFF) continue;

            //Check class code for NVMe Express controller (0x01 = Mass Storage, 0x08 = NVM, 0x02 = NVMe)
            uint8_t class_code = pci_read_config(b,s,0,0x0B);
            uint8_t subclass = pci_read_config(b,s,0,0x0A);
            uint8_t prog_if = pci_read_config(b,s,0,0x09);

            if(class_code == 0x01 && subclass == 0x08 && prog_if == 0x02){
                *bus = b;
                *slot = s;
                *func = 0;
                return 1;
            }
        }
    }
    return 0;
}

static kerr_t nvme_init_queue_pair(nvme_queue_pair_t* qp, uint16_t sq_size, uint16_t cq_size){
    //Allocate submission queue
    qp->sq = (nvme_sq_entry_t*)kmalloc(sq_size * sizeof(nvme_sq_entry_t));
    if(!qp->sq) return E_NOMEM;
    memset(qp->sq, 0, sq_size * sizeof(nvme_sq_entry_t));
    qp->sq_phys = (uint64_t)qp->sq;

    //Allocate completion queue
    qp->cq = (nvme_cq_entry_t*)kmalloc(cq_size * sizeof(nvme_cq_entry_t));
    if(!qp->cq) {
        kfree(qp->sq);
        return E_NOMEM;
    }
    memset(qp->cq, 0, cq_size * sizeof(nvme_cq_entry_t));
    qp->cq_phys = (uint64_t)qp->cq;

    qp->sq_size = sq_size;
    qp->cq_size = cq_size;
    qp->sq_tail = 0;
    qp->cq_head = 0;
    qp->cq_phase = 1;

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
    nvme_sq_entry_t cmd = {0};

    cmd.cdw0 = NVME_ADMIN_CREATE_CQ;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.prp1 = ctrl->io_queue.cq_phys;
    cmd.cdw10 = ((ctrl->io_queue.cq_size - 1) << 16) | 1;  // Queue size and ID
    cmd.cdw11 = 0x1;  // Physically contiguous

    nvme_submit_command(ctrl, &ctrl->admin_queue, &cmd, 1);
    return nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd.cdw0 >> 16) & 0xFFFF, 1);
}


//Create I/O submission queue
static kerr_t nvme_create_io_sq(nvme_controller_t* ctrl) {
    nvme_sq_entry_t cmd = {0};

    cmd.cdw0 = NVME_ADMIN_CREATE_SQ;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.prp1 = ctrl->io_queue.sq_phys;
    cmd.cdw10 = ((ctrl->io_queue.sq_size - 1) << 16) | 1;
    cmd.cdw11 = (1 << 16) | 0x1;

    nvme_submit_command(ctrl, &ctrl->admin_queue, &cmd, 1);
    return nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd.cdw0 >> 16) & 0xFFFF,1);
}

//Identify controller
int nvme_identify_controller(nvme_controller_t* ctrl, nvme_identify_controller_t* id) {
    void* buffer = kmalloc(4096);
    if(!buffer) return E_NOMEM;
    memset(buffer, 0, 4096);

    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = NVME_ADMIN_IDENTIFY;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.nsid = 0;
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = NVME_IDENTIFY_CONTROLLER;

    nvme_submit_command(ctrl, &ctrl->admin_queue, &cmd, 1);
    kerr_t err = nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd.cdw0 >> 16) & 0xFFFF, 1);

    if (err == E_OK){
        //Copy data
        uint8_t* src = (uint8_t*)buffer;
        uint8_t* dst = (uint8_t*)id;
        for(size_t i = 0; i < sizeof(nvme_identify_controller_t); i++){
            dst[i] = src[i];
        }

    }
    kfree(buffer);
    return err;
}

//Identify namespace
int nvme_identify_namespace(nvme_controller_t* ctrl, uint32_t nsid, nvme_identify_namespace_t* id) {
    void* buffer = kmalloc(4096);
    if(!buffer) return E_NOMEM;
    memset(buffer, 0, 4096);

    nvme_sq_entry_t cmd = {0};
    cmd.cdw0 = NVME_ADMIN_IDENTIFY;
    cmd.cdw0 |= (ctrl->command_id++ << 16);
    cmd.nsid = nsid;
    cmd.prp1 = (uint64_t)buffer;
    cmd.cdw10 = NVME_IDENTIFY_NAMESPACE;

    nvme_submit_command(ctrl, &ctrl->admin_queue, &cmd, 1);
    kerr_t err = nvme_wait_completion(ctrl, &ctrl->admin_queue, (cmd.cdw0 >> 16) & 0xFFFF, 1);

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

    //Enable PCI bus mastering and memory space
    uint16_t command = pci_read_config(bus, slot, func, PCI_COMMAND);
    command |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;
    pci_write_config(bus, slot, func, PCI_COMMAND, command);

    //Get BAR0
    uint32_t bar0_low = pci_read_config(bus, slot, func, PCI_BAR0);
    uint32_t bar0_high = pci_read_config(bus, slot, func, PCI_BAR1);
    uint64_t bar0 = ((uint64_t)bar0_high << 32) | (bar0_low & 0xFFFFFFF0);
    nvme_ctrl.bar0 = (volatile uint8_t*) bar0;

    //Disable controller
    uint32_t cc = nvme_read32(&nvme_ctrl, NVME_REG_CC);
    cc &= ~NVME_CC_ENABLE;
    nvme_write32(&nvme_ctrl, NVME_REG_CC, cc);

    //Wait for controller to be disabled
    uint32_t timeout = 10e6;
    while (timeout-- && (nvme_read32(&nvme_ctrl,NVME_CSTS_CFS) & NVME_CSTS_RDY));

    // Initialize admin queues
    if (nvme_init_queue_pair(&nvme_ctrl.admin_queue, NVME_ADMIN_QUEUE_SIZE,
                             NVME_ADMIN_QUEUE_SIZE) != E_OK) {
        return E_HARDWARE;
    }

    // Set admin queue addresses
    nvme_write64(&nvme_ctrl, NVME_REG_ASQ, nvme_ctrl.admin_queue.sq_phys);
    nvme_write64(&nvme_ctrl, NVME_REG_ACQ, nvme_ctrl.admin_queue.cq_phys);

    // Set admin queue sizes
    uint32_t aqa = ((NVME_ADMIN_QUEUE_SIZE - 1) << 16) | (NVME_ADMIN_QUEUE_SIZE - 1);
    nvme_write32(&nvme_ctrl, NVME_REG_AQA, aqa);

    // Configure and enable controller
    cc = NVME_CC_ENABLE | NVME_CC_CSS_NVM | NVME_CC_IOSQES | NVME_CC_IOCQES;
    nvme_write32(&nvme_ctrl, NVME_REG_CC, cc);

    // Wait for controller to be ready
    timeout = 1000000;
    while (timeout-- && !(nvme_read32(&nvme_ctrl, NVME_REG_CSTS) & NVME_CSTS_RDY));

    if (!(nvme_read32(&nvme_ctrl, NVME_REG_CSTS) & NVME_CSTS_RDY)) {
        return E_HARDWARE;
    }

    nvme_ctrl.command_id = 0;

    // Identify controller
    nvme_identify_controller_t ctrl_id;
    if (nvme_identify_controller(&nvme_ctrl, &ctrl_id) != E_OK) {
        return E_NOTDIR;
    }

    nvme_ctrl.num_namespaces = ctrl_id.nn;

    // Initialize I/O queues
    if (nvme_init_queue_pair(&nvme_ctrl.io_queue, NVME_IO_QUEUE_SIZE,
                             NVME_IO_QUEUE_SIZE) != E_OK) {
        return E_HARDWARE;
    }

    if (nvme_create_io_cq(&nvme_ctrl) != E_OK || nvme_create_io_sq(&nvme_ctrl) != E_OK) {
        return E_HARDWARE;
    }

    // Enumerate namespaces
    for (uint32_t i = 0; i < nvme_ctrl.num_namespaces && i < NVME_MAX_NAMESPACES; i++) {
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
            }
        }
    }
    return E_OK;
}