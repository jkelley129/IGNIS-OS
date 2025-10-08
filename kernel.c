#include "io/vga.h"
#include "io/idt.h"
#include "drivers/keyboard.h"
#include "drivers/pit.h"
#include "shell.h"

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
    vga_puts("Initializing IDT...\n");
    idt_init();
    vga_puts_color("IDT initialized!\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts("Initializing keyboard...\n");
    keyboard_init();
    vga_puts_color("Keyboard initialized!\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts("Initializing PIT\n");
    pit_init(100);
    vga_puts_color("PIT initialized", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    vga_puts_color("Ready! System is running.\n", (vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});

    asm volatile("sti");

    keyboard_set_callback(shell_handle_char);
    shell_init();

    // Infinite loop - interrupts will handle keyboard input
    while(1) {
        asm volatile("hlt");
    }
}