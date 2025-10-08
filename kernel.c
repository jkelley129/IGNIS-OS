#include "io/vga.h"
#include "io/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "shell.h"
#include "mm/memory.h"
#include "fs/vfs.h"

// Define heap area - 1MB heap starting at 2MB
#define HEAP_START 0x200000
#define HEAP_SIZE  0x100000  // 1MB

void kernel_main() {
    // Initialize the VGA text mode
    vga_init();
    vga_puts("Welcome!\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_RED, VGA_COLOR_BLACK});
    vga_puts("IGNIS v0.0.01\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK});
    vga_puts("---- Developed by Josh Kelley ----\n\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_WHITE, VGA_COLOR_BLACK});

    // Initialize interrupts and keyboard
    vga_puts("Initializing IDT...  ");
    idt_init();
    vga_puts_color("[SUCCESS]\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts("Initializing Memory...  ");
    memory_init(HEAP_START, HEAP_SIZE);
    vga_puts_color("[SUCCESS]\n", (vga_color_attr_t) {VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts("Initializing RAM File System...  ");
    vfs_init();
    vga_puts_color("[SUCCESS]\n", (vga_color_attr_t) {VGA_COLOR_GREEN, VGA_COLOR_BLACK});
    vga_puts("Initializing keyboard...  ");
    keyboard_init();
    vga_puts_color("[SUCCESS]\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts("Initializing PIT...  ");
    pit_init(100);
    vga_puts_color("[SUCCESS]\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts_color("\nReady! System is running.\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    //Enable interrupts
    asm volatile("sti");

    keyboard_set_callback(shell_handle_char);
    shell_init();

    // Infinite loop - interrupts will handle keyboard input
    while(1) {
        asm volatile("hlt");
    }
}