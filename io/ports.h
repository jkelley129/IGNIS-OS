#ifndef PORTS_H
#define PORTS_H

#include "../libc/stdint.h"

// Read a byte from a port
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write a byte to a port
static inline void outb(uint16_t port, uint8_t data) {
    asm volatile("outb %0, %1" : : "a"(data), "Nd"(port));
}

// Read 16 bits from a port
static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write 16 bits to a port
static inline void outw(uint16_t port, uint16_t data) {
    asm volatile("outw %0, %1" : : "a"(data), "Nd"(port));
}

// Read 32 bits from a port
static inline uint32_t inl(uint16_t port) {
    uint32_t result;
    asm volatile("inl %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

// Write 32 bits to a port
static inline void outl(uint16_t port, uint32_t data) {
    asm volatile("outl %0, %1" : : "a"(data), "Nd"(port));
}

#endif