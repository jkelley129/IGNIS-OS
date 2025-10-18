#include "driver.h"
#include "console/console.h"
#include "interrupts/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "drivers/block.h"
#include "drivers/disks/ata.h"
#include "shell/shell.h"
#include "mm/memory.h"
#include "fs/vfs.h"
#include "error_handling/errno.h"
#include "vga.h"
#include "fs/filesystems/ramfs.h"

// Define heap area - 1MB heap starting at 2MB
#define HEAP_START 0x200000
#define HEAP_SIZE  0x100000  // 1MB

#define COLOR_SUCCESS (console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK}
#define COLOR_FAILURE (console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK}

void kernel_main() {
    // Initialize the VGA text mode
    console_init(vga_get_driver());
    console_puts("Welcome!\n");
    console_puts_color("IGNIS v0.0.01\n", (console_color_attr_t) {CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
    console_puts_color("---- Developed by Josh Kelley ----\n\n",(console_color_attr_t) {CONSOLE_COLOR_LIGHT_BLUE, CONSOLE_COLOR_BLACK});

    //Define error count
    uint8_t err_count = 0;
    kerr_t status;

    // Initialize interrupts and keyboard
    TRY_INIT("IDT", idt_init(), err_count)

    TRY_INIT("Memory", memory_init(HEAP_START, HEAP_SIZE),err_count)

    // Initialize VFS layer
    TRY_INIT("VFS Layer", vfs_init(), err_count)

    // Create and mount RAM filesystem
    console_puts("Mounting RAM File System...   ");
    filesystem_t* ramfs = NULL;
    status = ramfs_create_fs(&ramfs);
    if (status == E_OK) {
        status = vfs_mount(ramfs, "/");
        if (status == E_OK) {
            console_puts_color("[SUCCESS]\n", COLOR_SUCCESS);
        } else {
            k_pkerr(status);
            err_count++;
        }
    } else {
        k_pkerr(status);
        err_count++;
    }

    TRY_INIT("Keyboard", keyboard_init(), err_count)

    TRY_INIT("PIT", pit_init(100), err_count)

    TRY_INIT("Block Device Layer",block_init(),err_count)

    TRY_INIT("ATA",ata_init(),err_count)

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
    driver_list();

    //Enable interrupts
    asm volatile("sti");

    keyboard_set_callback(shell_handle_char);
    shell_init();

    // Infinite loop - interrupts will handle all input for now
    while(1) {
        asm volatile("hlt");
    }
}