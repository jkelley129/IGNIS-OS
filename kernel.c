#include "driver.h"
#include "console/console.h"
#include "disks/nvme.h"
#include "interrupts/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "drivers/block.h"
#include "drivers/disks/ata.h"
#include "shell/shell.h"
#include "mm/memory.h"
#include "mm/allocators/buddy.h"
#include "mm/allocators/slab.h"
#include "mm/memory_layout.h"
#include "fs/vfs.h"
#include "error_handling/errno.h"
#include "io/vga.h"
#include "io/serial.h"
#include "fs/filesystems/ramfs.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "scheduler/task.h"

// Define heap area - 1MB heap starting at 2MB
#define HEAP_START 0x200000
#define HEAP_SIZE  0x100000  // 1MB

#define COLOR_SUCCESS (console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK}
#define COLOR_FAILURE (console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK}

// Shell task entry point
void shell_task_entry(void) {
    serial_debug_puts("Shell task started\n");

    // Initialize shell
    shell_init();

    // Shell runs forever, processing input
    while(1) {
        // Shell is driven by keyboard interrupts
        // Just yield CPU when nothing to do
        task_yield();
    }
}

void kernel_main() {
    //init serial first for debugging things later
    kerr_t serial_status = serial_init(COM1);

    // Initialize the VGA text mode
    console_init(vga_get_driver());

    // Test serial output
    serial_debug_puts("=== IGNIS OS Serial Debug Log ===\n");
    serial_debug_puts("Serial port initialized successfully\n");
    serial_debug_puts("Starting kernel initialization...\n\n");

    // Log kernel addresses
    serial_debug_puts("Kernel virtual base: ");
    serial_puthex(COM1, VIRT_KERNEL_BASE, 16);
    serial_debug_puts("\n");

    serial_debug_puts("Starting kernel init\n");

    console_puts("Welcome!\n");
    console_puts_color("IGNIS v0.0.01\n", (console_color_attr_t) {CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
    console_puts_color("---- Developed by Josh Kelley ----\n\n",(console_color_attr_t) {CONSOLE_COLOR_LIGHT_BLUE, CONSOLE_COLOR_BLACK});

    if (serial_status == E_OK) {
        console_puts("Serial port: COM1 (see serial.log)\n");
    }

    //Define error count
    uint8_t err_count = 0;
    kerr_t status;

    // Initialize interrupts
    TRY_INIT("IDT", idt_register(), err_count)

    TRY_INIT("Memory", memory_init(PHYS_HEAP_START, PHYS_HEAP_SIZE),err_count)

    TRY_INIT("PMM", pmm_init(), err_count)
    TRY_INIT("VMM", vmm_init(), err_count)

    buddy_allocator_t* buddy = kalloc_pages(1);
    TRY_INIT("Buddy Alloc",buddy_init(buddy, 0x04000000, 0x04000000),err_count)  // 64MB
    TRY_INIT("Slab Alloc",slab_init(),err_count)

    // Initialize VFS layer
    TRY_INIT("VFS Layer", vfs_init(), err_count)

    // Create and mount RAM filesystem
    serial_debug_puts("Mounting RAM File System... ");
    console_puts("Mounting RAM File System...   ");
    filesystem_t* ramfs = NULL;
    status = ramfs_create_fs(&ramfs);
    if (status == E_OK) {
        status = vfs_mount(ramfs, "/");
        if (status == E_OK) {
            serial_debug_puts("[SUCCESS]\n");
            console_puts_color("[SUCCESS]\n", COLOR_SUCCESS);
        } else {
            k_pkerr(status);
            err_count++;
        }
    } else {
        k_pkerr(status);
        err_count++;
    }

    //Initialize drivers
    TRY_INIT("Keyboard", keyboard_register(), err_count)

    TRY_INIT("PIT", pit_register(100), err_count)

    TRY_INIT("Block Device Layer",block_register(),err_count)

    TRY_INIT("ATA",ata_register(),err_count)

    TRY_INIT("NVMe",nvme_register(), err_count);

    if(err_count == 0) console_puts_color("\nReady! System is running.\n", COLOR_SUCCESS);
    else{
        char num_str[3];
        uitoa(err_count, num_str);
        console_set_color(COLOR_FAILURE);
        console_puts("\nWARNING! ");
        console_puts(num_str);
        console_puts(" Initialization(s) failed!\n\n");
        console_set_color((console_color_attr_t) {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    driver_init_all();

    // Initialize task system BEFORE enabling interrupts
    TRY_INIT("Task System", task_init(), err_count)
    TRY_INIT("Scheduler", scheduler_init(), err_count)

    // Create shell task
    task_t* shell_task = task_create("shell", shell_task_entry);
    if (shell_task) {
        scheduler_add_task(shell_task);
        serial_debug_puts("Shell task created and added to scheduler\n");
        console_puts("Shell task created\n");
    } else {
        console_perror("Failed to create shell task!\n");
        err_count++;
    }

    driver_list();

    // Set keyboard callback to shell
    keyboard_set_callback(shell_handle_char);

    // NOW enable interrupts - scheduler is ready
    idt_enable_interrupts();

    serial_debug_puts("Kernel initialization complete, entering idle loop\n");
    console_puts("\nKernel running. Type 'help' for commands.\n\n");

    // Kernel becomes the idle task - just halt and wait for interrupts
    while(1) {
        asm volatile("hlt");
    }
}