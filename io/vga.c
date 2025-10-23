#include "vga.h"
#include "../console/console.h"
#include "mm/memory_layout.h"

// VGA-specific state
static uint16_t* vga_hardware_buffer = (uint16_t*)PHYS_TO_VIRT(0xB8000);
static uint16_t vga_virtual_buffer[VGA_WIDTH * VGA_BUFFER_HEIGHT];
static uint16_t vga_cursor = 0;
static uint16_t vga_scroll_offset = 0;  // Top line currently visible
static console_color_attr_t vga_color = {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK};

// Forward declarations of driver functions
static kerr_t vga_driver_init(void);
static void vga_driver_clear(void);
static void vga_driver_putc(char c);
static void vga_driver_puts(const char* str);
static void vga_driver_set_color(console_color_attr_t color);
static console_color_attr_t vga_driver_get_color(void);
static void vga_driver_backspace(int count);

// Helper function to refresh the hardware buffer from virtual buffer
static void vga_refresh_screen(void);

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
  uint16_t blank = (vga_color.background << 12) | (vga_color.foreground << 8) | ' ';

  // Clear virtual buffer
  for (uint16_t i = 0; i < VGA_WIDTH * VGA_BUFFER_HEIGHT; i++) {
    vga_virtual_buffer[i] = blank;
  }

  vga_cursor = 0;
  vga_scroll_offset = 0;
  vga_refresh_screen();
}

static void vga_refresh_screen(void) {
  // if ()

  // Copy the visible portion of virtual buffer to hardware buffer
  for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    uint16_t virtual_index = (vga_scroll_offset * VGA_WIDTH) + i;
    vga_hardware_buffer[i] = vga_virtual_buffer[virtual_index];
  }
}

static void vga_driver_putc(char c) {
  if (c == '\n') {
    // Move to start of next line
    vga_cursor = ((vga_cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
  } else {
    // Write character to virtual buffer
    vga_virtual_buffer[vga_cursor] = (vga_color.background << 12) |
                                      (vga_color.foreground << 8) | c;
    vga_cursor++;
  }

  // Check if we need to scroll
  uint16_t current_line = vga_cursor / VGA_WIDTH;

  if (current_line >= VGA_BUFFER_HEIGHT) {
    // We've reached the end of the virtual buffer, need to scroll it
    // Shift all lines up by one
    for (uint16_t i = 0; i < VGA_WIDTH * (VGA_BUFFER_HEIGHT - 1); i++) {
      vga_virtual_buffer[i] = vga_virtual_buffer[i + VGA_WIDTH];
    }

    // Clear the last line
    uint16_t blank = (vga_color.background << 12) | (vga_color.foreground << 8) | ' ';
    for (uint16_t i = VGA_WIDTH * (VGA_BUFFER_HEIGHT - 1); i < VGA_WIDTH * VGA_BUFFER_HEIGHT; i++) {
      vga_virtual_buffer[i] = blank;
    }

    // Move cursor back one line
    vga_cursor -= VGA_WIDTH;
    current_line--;

    // Adjust scroll offset if needed
    if (vga_scroll_offset > 0) {
      vga_scroll_offset--;
    }
  }

  // Auto-scroll if cursor is beyond visible area
  if (current_line >= vga_scroll_offset + VGA_HEIGHT) {
    vga_scroll_offset = current_line - VGA_HEIGHT + 1;
  }

  vga_refresh_screen();
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
  if (vga_cursor > 0) {
    vga_cursor--;

    uint16_t blank = (vga_color.background << 12) | (vga_color.foreground << 8) | ' ';
    for (int i = 0; i <= count && vga_cursor < VGA_WIDTH * VGA_BUFFER_HEIGHT; i++) {
      vga_virtual_buffer[vga_cursor] = blank;
      if (vga_cursor > 0 && i < count) {
        vga_cursor--;
      }
    }

    vga_refresh_screen();
  }
}