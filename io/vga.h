#ifndef VGA_H
#define VGA_H

#include "../libc/stdint.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_MEMORY 0xB8000

typedef enum {
  VGA_COLOR_BLACK = 0,
  VGA_COLOR_BLUE = 1,
  VGA_COLOR_GREEN = 2,
  VGA_COLOR_CYAN = 3,
  VGA_COLOR_RED = 4,
  VGA_COLOR_MAGENTA = 5,
  VGA_COLOR_BROWN = 6,
  VGA_COLOR_LIGHT_GREY = 7,
  VGA_COLOR_DARK_GREY = 8,
  VGA_COLOR_LIGHT_BLUE = 9,
  VGA_COLOR_LIGHT_GREEN = 10,
  VGA_COLOR_LIGHT_CYAN = 11,
  VGA_COLOR_LIGHT_RED = 12,
  VGA_COLOR_LIGHT_MAGENTA = 13,
  VGA_COLOR_LIGHT_BROWN = 14,
  VGA_COLOR_WHITE = 15,
} vga_color_t;

typedef struct {
  uint8_t foreground : 4;
  uint8_t background : 4;
}vga_color_attr_t;

void vga_init();
void vga_clear();
void vga_putc(char c);
void vga_puts(const char* str);
void vga_puts_color(const char* str, vga_color_attr_t);
void vga_backspace(int count);
void vga_perror(const char* error_str);
void vga_set_color(vga_color_attr_t color);

#endif
