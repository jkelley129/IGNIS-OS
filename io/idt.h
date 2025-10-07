#ifndef IDT_H
#define IDT_H

#include "../libc/stdint.h"

#define IDT_ENTRIES 256

// 64-bit IDT entry is 16 bytes (not 8)
typedef struct {
    uint16_t base_low;      // Lower 16 bits of handler address
    uint16_t selector;      // Kernel segment selector
    uint8_t ist;            // Interrupt Stack Table offset (usually 0)
    uint8_t flags;          // Type and attributes
    uint16_t base_mid;      // Middle 16 bits of handler address
    uint32_t base_high;     // Upper 32 bits of handler address
    uint32_t reserved;      // Reserved, must be 0
} __attribute__((packed)) idt_entry_t;

typedef struct {
    uint16_t limit;
    uint64_t base;          // Changed from uint32_t to uint64_t
} __attribute__((packed)) idt_ptr_t;

void idt_init();
void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags);

#endif