#include "io/vga.h"
#include "interrupts/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "drivers/block.h"
#include "drivers/disks/ata.h"
#include "drivers/disks/nvme.h"
#include "shell/shell.h"
#include "mm/memory.h"
#include "fs/vfs.h"
#include "error_handling/errno.h"

// Define heap area - 1MB heap starting at 2MB
#define HEAP_START 0x200000
#define HEAP_SIZE  0x100000  // 1MB

#define COLOR_SUCCESS (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK}
#define COLOR_FAILURE (vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK}

void kernel_main() {
    // Initialize the VGA text mode
    vga_init();
    vga_puts("Welcome!\n");
    vga_puts_color("IGNIS v0.0.01\n", (vga_color_attr_t) {VGA_COLOR_RED, VGA_COLOR_BLACK});
    vga_puts_color("---- Developed by Josh Kelley ----\n\n",(vga_color_attr_t) {VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK});

    //Define error count
    uint8_t err_count = 0;

    // Initialize interrupts and keyboard
    TRY_INIT("IDT", idt_init(), err_count)

    TRY_INIT("Memory", memory_init(HEAP_START, HEAP_SIZE), err_count)

    TRY_INIT("RAM File System",vfs_init(),err_count)

    TRY_INIT("keyboard", keyboard_init(), err_count);

    TRY_INIT("PIT", pit_init(100), err_count)

    TRY_INIT("Block Device Layer",block_init(),err_count);

    TRY_INIT("ATA",ata_init(),err_count)

    TRY_INIT("NVMe", nvme_init(),err_count)

    if(err_count == 0) vga_puts_color("\nReady! System is running.\n", COLOR_SUCCESS);
    else{
        char num_str[3];
        uitoa(err_count, num_str);
        vga_set_color(COLOR_FAILURE);
        vga_puts("\nWARNING! ");
        vga_puts(num_str);
        vga_puts(" Initialization(s) failed!\n\n");
        vga_set_color((vga_color_attr_t) {VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    }

    //Enable interrupts
    asm volatile("sti");

    keyboard_set_callback(shell_handle_char);
    shell_init();

    // Infinite loop - interrupts will handle all input for now
    while(1) {
        asm volatile("hlt");
    }
}