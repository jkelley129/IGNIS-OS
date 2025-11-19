#include "vga.h"
#include "console/console.h"
#include "mm/memory_layout.h"

// VGA-specific state
static uint16_t* vga_hardware_buffer = (uint16_t*)PHYS_TO_VIRT(0xB8000);
static uint16_t vga_virtual_buffer[VGA_WIDTH * VGA_BUFFER_HEIGHT];
static uint16_t vga_cursor = 0;
static uint16_t vga_scroll_offset = 0;
static console_color_attr_t vga_color = {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK};

static uint8_t dirty_lines[VGA_HEIGHT];
static uint8_t needs_refresh = 0;

// Forward declarations
static kerr_t vga_driver_init(void);
static void vga_driver_clear(void);
static void vga_driver_putc(char c);
static void vga_driver_puts(const char* str);
static void vga_driver_set_color(console_color_attr_t color);
static console_color_attr_t vga_driver_get_color(void);
static void vga_driver_backspace(int count);

static void vga_refresh_screen(void);
static void vga_driver_flush(void);

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
    // Clear dirty line tracking
    for (int i = 0; i < VGA_HEIGHT; i++) {
        dirty_lines[i] = 0;
    }
    needs_refresh = 0;

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

    // Mark all lines dirty and refresh immediately
    for (int i = 0; i < VGA_HEIGHT; i++) {
        dirty_lines[i] = 1;
    }
    vga_refresh_screen();
    needs_refresh = 0;
}

static void vga_refresh_screen(void) {
    if (!needs_refresh) return;

    for (uint16_t line = 0; line < VGA_HEIGHT; line++) {
        if (dirty_lines[line]) {
            // Only copy this line
            uint16_t virtual_start = (vga_scroll_offset + line) * VGA_WIDTH;
            uint16_t hardware_start = line * VGA_WIDTH;

            for (uint16_t col = 0; col < VGA_WIDTH; col++) {
                vga_hardware_buffer[hardware_start + col] = vga_virtual_buffer[virtual_start + col];
            }

            dirty_lines[line] = 0;
        }
    }

    needs_refresh = 0;
}

static void vga_driver_flush(void) {
    vga_refresh_screen();
}

static inline void mark_line_dirty(uint16_t cursor_pos) {
    uint16_t virtual_line = cursor_pos / VGA_WIDTH;
    uint16_t visible_line = virtual_line - vga_scroll_offset;

    if (visible_line < VGA_HEIGHT) {
        dirty_lines[visible_line] = 1;
        needs_refresh = 1;
    }
}

static void vga_driver_putc(char c) {
    uint16_t old_cursor = vga_cursor;

    if (c == '\n') {
        // Move to start of next line
        vga_cursor = ((vga_cursor / VGA_WIDTH) + 1) * VGA_WIDTH;
    } else {
        // Write character to virtual buffer
        vga_virtual_buffer[vga_cursor] = (vga_color.background << 12) |
                                          (vga_color.foreground << 8) | c;
        vga_cursor++;
    }

    // Mark affected lines as dirty
    mark_line_dirty(old_cursor);
    if (vga_cursor / VGA_WIDTH != old_cursor / VGA_WIDTH) {
        mark_line_dirty(vga_cursor);
    }

    // Check if we need to scroll
    uint16_t current_line = vga_cursor / VGA_WIDTH;

    if (current_line >= VGA_BUFFER_HEIGHT) {
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

        // Adjust scroll offset
        if (vga_scroll_offset > 0) {
            vga_scroll_offset--;
        }

        // Mark all lines dirty after scroll
        for (int i = 0; i < VGA_HEIGHT; i++) {
            dirty_lines[i] = 1;
        }
        needs_refresh = 1;
    }

    // Auto-scroll if cursor is beyond visible area
    if (current_line >= vga_scroll_offset + VGA_HEIGHT) {
        vga_scroll_offset = current_line - VGA_HEIGHT + 1;

        // Mark all lines dirty after scroll
        for (int i = 0; i < VGA_HEIGHT; i++) {
            dirty_lines[i] = 1;
        }
        needs_refresh = 1;
    }

    if (c == '\n' || needs_refresh) {
        vga_refresh_screen();
    }
}

static void vga_driver_puts(const char* str) {
    while (*str) {
        vga_driver_putc(*str);
        str++;
    }

    vga_driver_flush();
}

static void vga_driver_set_color(console_color_attr_t color) {
    vga_color = color;
}

static console_color_attr_t vga_driver_get_color(void) {
    return vga_color;
}

static void vga_driver_backspace(int count) {
    if (vga_cursor > 0) {
        uint16_t old_cursor = vga_cursor;
        vga_cursor--;

        uint16_t blank = (vga_color.background << 12) | (vga_color.foreground << 8) | ' ';
        for (int i = 0; i <= count && vga_cursor < VGA_WIDTH * VGA_BUFFER_HEIGHT; i++) {
            vga_virtual_buffer[vga_cursor] = blank;
            mark_line_dirty(vga_cursor);
            if (vga_cursor > 0 && i < count) {
                vga_cursor--;
            }
        }

        // Mark affected lines
        mark_line_dirty(old_cursor);
        mark_line_dirty(vga_cursor);

        // Refresh after backspace
        vga_refresh_screen();
    }
}