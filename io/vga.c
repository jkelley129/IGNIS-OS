#include "vga.h"

static uint16_t* vga_buffer =  (uint16_t*) VGA_MEMORY;
static uint16_t vga_cursor = 0;
static vga_color_attr_t vga_color = {VGA_COLOR_WHITE, VGA_COLOR_BLACK};

void vga_init(){
  vga_clear();
}

void vga_clear(){
  for (uint16_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++){
    vga_buffer[i] = (vga_color.background << 12) | (vga_color.foreground << 8);
  } 
  vga_cursor = 0;
}

void vga_putc(char c){
  if(c == '\n') {
    vga_cursor = ((vga_cursor + VGA_WIDTH) / VGA_WIDTH * VGA_WIDTH);
  }else{
    vga_buffer[vga_cursor] = (vga_color.background << 12) | (vga_color.foreground << 8) | c;
    vga_cursor++;
  }

  if(vga_cursor >= VGA_WIDTH * VGA_HEIGHT) {
    vga_clear();
  }
}

void vga_puts(const char* str){
  while (*str){
    vga_putc(*str);
    str++;
  }
}

void vga_backspace(int count){
  vga_cursor--;
  for(int i = 0; i <= count; i++){
    vga_putc(0);
    vga_cursor--;
  }
}

void vga_set_color(vga_color_attr_t color){
  vga_color = color;
}
