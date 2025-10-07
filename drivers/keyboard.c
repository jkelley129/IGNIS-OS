#include "keyboard.h"
#include "../io/vga.h"

// US QWERTY keyboard layout scancode to ASCII
static const char scancode_to_ascii[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
        '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
        0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
        0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
        '*', 0, ' '
};

static const char scancode_to_ascii_shift[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
        '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
        0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
        0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
        '*', 0, ' '
};

static uint8_t shift_pressed = 0;
static keyboard_callback_t key_callback = 0;

void keyboard_init() {
    // Keyboard is initialized by the BIOS, nothing needed
}

void keyboard_set_callback(keyboard_callback_t callback) {
    key_callback = callback;
}

void keyboard_handler() {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    // Handle shift keys
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = 0;
        return;
    }

    if(scancode == 0x0E) {
        if (key_callback) {
            key_callback('\b');
            vga_backspace(1);
        } else {
            vga_backspace(1);
        }
        return;
    }

    // Ignore key releases (scancode >= 0x80)
    if (scancode >= 0x80) {
        return;
    }

    // Convert scancode to ASCII
    char c = 0;
    if (scancode < sizeof(scancode_to_ascii)) {
        if (shift_pressed) {
            c = scancode_to_ascii_shift[scancode];
        } else {
            c = scancode_to_ascii[scancode];
        }
    }

    // Print the character if valid
    if (c) {
        if (key_callback) {
            key_callback(c);
            vga_putc(c);
        } else {
            vga_putc(c);
        }
    }
}