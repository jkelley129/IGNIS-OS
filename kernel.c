#include "io/vga.h"

void kernel_main() {
    // Initialize the VGA text mode
    vga_init();
    vga_puts("Welcome!\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_RED, VGA_COLOR_BLACK});
    vga_puts("IGNIS v0.0.01\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_GREEN, VGA_COLOR_BLACK});
    vga_puts("/*** Developed by Josh Kelley ***\\\n\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK});
    vga_puts("--- Feature List ---\n\n");
    vga_set_color((vga_color_attr_t) {VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    vga_puts("Colored Text Output");
}
