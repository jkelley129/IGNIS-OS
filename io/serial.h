#ifndef SERIAL_H
#define SERIAL_H

#include "libc/stddef.h"
#include "libc/stdint.h"
#include "error_handling/errno.h"

// COM port addresses
#define COM1 0x3F8
#define COM2 0x2F8
#define COM3 0x3E8
#define COM4 0x2E8

// Serial port registers (offset from base)
#define SERIAL_DATA          0  // Data register (read/write)
#define SERIAL_INT_ENABLE    1  // Interrupt enable
#define SERIAL_FIFO_CTRL     2  // FIFO control
#define SERIAL_LINE_CTRL     3  // Line control
#define SERIAL_MODEM_CTRL    4  // Modem control
#define SERIAL_LINE_STATUS   5  // Line status
#define SERIAL_MODEM_STATUS  6  // Modem status
#define SERIAL_SCRATCH       7  // Scratch register

// Line status register bits
#define SERIAL_LSR_DATA_READY    0x01
#define SERIAL_LSR_OVERRUN_ERROR 0x02
#define SERIAL_LSR_PARITY_ERROR  0x04
#define SERIAL_LSR_FRAMING_ERROR 0x08
#define SERIAL_LSR_BREAK         0x10
#define SERIAL_LSR_TX_EMPTY      0x20  // Transmitter holding register empty
#define SERIAL_LSR_TX_IDLE       0x40  // Transmitter empty (idle)

// Initialize serial port
kerr_t serial_init(uint16_t port);

// Write operations
void serial_putc(uint16_t port, char c);
void serial_puts(uint16_t port, const char* str);
void serial_write(uint16_t port, const char* data, size_t len);

// Read operations (for future use)
int serial_received(uint16_t port);
char serial_getc(uint16_t port);

// Convenience macros for COM1 (primary debug port)
#define serial_debug_putc(c)   serial_putc(COM1, c)
#define serial_debug_puts(s)   serial_puts(COM1, s)
#define serial_debug_write(d,l) serial_write(COM1, d, l)

//Convenience macros for both serial and console
#define multiple_putc(c) do { \
    serial_putc(COM1, c); \
    console_putc(c); \
}while (0)

#define multiple_puts(s) do { \
serial_puts(COM1, s); \
console_puts(s); \
}while (0)

// Hex output helper for debugging
void serial_puthex(uint16_t port, uint64_t value, int width);

#endif