#include "keyboard.h"
#include "console/console.h"
#include "io/ports.h"
#include "driver.h"
#include "error_handling/errno.h"
#include "libc/stddef.h"
#include "tty/tty.h"

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

// Forward declaration of driver init function
static kerr_t keyboard_driver_init(driver_t* drv);

// Driver structure
static driver_t keyboard_driver = {
    .name = "Keyboard",
    .type = DRIVER_TYPE_INPUT,
    .version = 1,
    .priority = 20,  // Initialize after IDT (priority 10)
    .init = keyboard_driver_init,
    .cleanup = NULL,
    .depends_on = "IDT",  // Depends on interrupt system
    .driver_data = NULL
};

// Driver initialization function
static kerr_t keyboard_driver_init(driver_t* drv) {
    // Keyboard is initialized by the BIOS, nothing special needed
    return E_OK;
}

// Public init function - registers the driver
kerr_t keyboard_register() {
    return driver_register(&keyboard_driver);
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

    // Handle backspace
    if(scancode == 0x0E) {
        tty_input_char('\b');
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

    // Send to TTY layer
    if (c) {
        tty_input_char(c);
    }
}