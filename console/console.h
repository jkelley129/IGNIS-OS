#ifndef CONSOLE_H
#define CONSOLE_H

#include "../libc/stdint.h"
#include "../error_handling/errno.h"

// Console color type (backend-agnostic)
typedef enum {
    CONSOLE_COLOR_BLACK = 0,
    CONSOLE_COLOR_BLUE = 1,
    CONSOLE_COLOR_GREEN = 2,
    CONSOLE_COLOR_CYAN = 3,
    CONSOLE_COLOR_RED = 4,
    CONSOLE_COLOR_MAGENTA = 5,
    CONSOLE_COLOR_BROWN = 6,
    CONSOLE_COLOR_LIGHT_GREY = 7,
    CONSOLE_COLOR_DARK_GREY = 8,
    CONSOLE_COLOR_LIGHT_BLUE = 9,
    CONSOLE_COLOR_LIGHT_GREEN = 10,
    CONSOLE_COLOR_LIGHT_CYAN = 11,
    CONSOLE_COLOR_LIGHT_RED = 12,
    CONSOLE_COLOR_LIGHT_MAGENTA = 13,
    CONSOLE_COLOR_LIGHT_BROWN = 14,
    CONSOLE_COLOR_WHITE = 15,
} console_color_t;

typedef struct {
    uint8_t foreground : 4;
    uint8_t background : 4;
} console_color_attr_t;

// Console driver operations
typedef struct console_driver {
    kerr_t (*init)(void);
    void (*clear)(void);
    void (*putc)(char c);
    void (*puts)(const char* str);
    void (*set_color)(console_color_attr_t color);
    console_color_attr_t (*get_color)(void);
    void (*backspace)(int count);
} console_driver_t;

// Console interface functions
kerr_t console_init(console_driver_t* driver);
void console_clear(void);
void console_putc(char c);
void console_puts(const char* str);
void console_puts_color(const char* str, console_color_attr_t color);
void console_set_color(console_color_attr_t color);
console_color_attr_t console_get_color(void);
void console_backspace(int count);
void console_perror(const char* error_str);

// Helper color definitions
#define CONSOLE_COLOR_DEFAULT (console_color_attr_t) {CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK}
#define CONSOLE_COLOR_SUCCESS (console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK}
#define CONSOLE_COLOR_FAILURE (console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK}
#define CONSOLE_COLOR_WARNING (console_color_attr_t){CONSOLE_COLOR_LIGHT_BROWN, CONSOLE_COLOR_BLACK}
#define CONSOLE_COLOR_INFO (console_color_attr_t){CONSOLE_COLOR_LIGHT_CYAN, CONSOLE_COLOR_BLACK}

#endif // CONSOLE_H