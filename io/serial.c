#include "serial.h"
#include "ports.h"
#include "libc/string.h"

static int serial_is_buffer_empty(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_TX_EMPTY;
}

kerr_t serial_init(uint16_t port) {
    //Disable interrupts
    outb(port + SERIAL_INT_ENABLE,0x00);

    //Enable DLAB (set baud rate divisor)
    outb(port + SERIAL_LINE_CTRL, 0x80);

    // Set divisor to 3 (38400 baud)
    outb(port + SERIAL_DATA, 0x03);
    outb(port + SERIAL_INT_ENABLE, 0x00);

    // 8 bits, no parity, one stop bit
    outb(port + SERIAL_LINE_CTRL, 0x03);

    // Enable FIFO, clear them, with 14-byte threshold
    outb(port + SERIAL_FIFO_CTRL, 0xC7);

    // IRQs enabled, RTS/DSR set
    outb(port + SERIAL_MODEM_CTRL, 0x0B);

    // Test serial chip (send byte 0xAE and check if serial returns same byte)
    outb(port + SERIAL_DATA, 0xAE);

    //Check if serial is faulty
    if (inb(port + SERIAL_DATA) != 0xAE) return E_HARDWARE;

    //Else, put it into normal operation mode
    outb(port + SERIAL_MODEM_CTRL, 0x0F);

    return E_OK;
}

void serial_putc(uint16_t port, const char c) {
    while (!serial_is_buffer_empty(port));

    //Send char
    outb(port + SERIAL_DATA, c);
}

void serial_puts(uint16_t port, const char* str) {
    while (*str) {
        if (*str == '\n') {
            serial_putc(port, '\r');
        }

        serial_putc(port, *str);
        str++;
    }
}

void serial_write(uint16_t port, const char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        serial_putc(port, data[i]);
    }
}

int serial_recieved(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY;
}

char serial_getc(uint16_t port) {
    while (!serial_recieved(port));
    return inb(port + SERIAL_DATA);
}

void serial_puthex(uint16_t port, uint64_t value, int width) {
    const char hex_chars[] = "0123456789ABCDEF";
    char buffer[17]; // Max 16 digits, plus null
    int i = 0;

    if (width > 16) width = 16;
    if (width <= 0) width = 1;

    //Convert to hex
    do {
        buffer[i++] = hex_chars[value & 0xF];
        value >>= 4;
    }while (value != 0 && i < width);

    // Pad with zeros if needed
    while (i < width) {
        buffer[i++] = '0';
    }

    // Output in correct order
    serial_puts(port, "0x");
    while (i > 0) {
        serial_putc(port, buffer[--i]);
    }
}