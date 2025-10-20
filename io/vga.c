#include "vga.h"
#include "../console/console.h"

// VGA-specific state
// VGA buffer is at physical 0xB8000, but we need to access it via higher-half
// For now, we'll keep using 0xB8000 since we have identity mapping in place
// TODO: Once we have proper virtual memory, map VGA buffer properly
static uint16_t* vga_buffer = (uint16_t*)0xB8000ULL;
static uint16_t vga_cursor = 0;
static console_color_attr_t vga_color = {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK};

// Forward declarations of driver functions
static kerr_t vga_driver_init(void);
static void vga_driver_clear(void);
static void vga_driver_putc(char c);
static void vga_driver_puts(const char* str);
static void vga_driver_set_color(console_color_attr_t color);
static console_color_attr_t vga_driver_get_color(void);
static void vga_driver_backspace(int count);

static console_driver_t vga_driver = {
        .init = vga_driver_init,
        .clear = vga_driver_clear,
        .putc = vga_driver_putc,
        .puts = vga_driver_puts,
        .set_color = vga_driver_set_color,
        .get_color = vga_driver_get_color,
        .backspace = vga_driver_backspace
};

console_driver_t* vga_get_driver(void) {
  return &vga_driver;
}

static kerr_t vga_driver_init(void){
    vga_driver_clear();
    return E_OK;
}

static void vga_driver_clear(void) {
  for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    vga_buffer[i] = (vga_color.background << 12) | (vga_color.foreground << 8);
  }
  vga_cursor = 0;
}

static void vga_driver_putc(char c) {
  if (c == '\n') {
    vga_cursor = ((vga_cursor + VGA_WIDTH) / VGA_WIDTH * VGA_WIDTH);
  } else {
    vga_buffer[vga_cursor] = (vga_color.background << 12) |
                             (vga_color.foreground << 8) | c;
    vga_cursor++;
  }

  if (vga_cursor >= VGA_WIDTH * VGA_HEIGHT) {
    vga_driver_clear();
  }
}

static void vga_driver_puts(const char* str) {
  while (*str) {
    vga_driver_putc(*str);
    str++;
  }
}

static void vga_driver_set_color(console_color_attr_t color) {
  vga_color = color;
}

static console_color_attr_t vga_driver_get_color(void) {
    return vga_color;
}

static void vga_driver_backspace(int count) {
  vga_cursor--;
  for (int i = 0; i <= count; i++) {
    vga_driver_putc(0);
    vga_cursor--;
  }
}