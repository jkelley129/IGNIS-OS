#include "idt.h"
#include "ports.h"

// External assembly functions
extern void idt_load(uint64_t);
extern void irq0();
extern void irq1();
extern void irq_default();

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

void idt_set_gate(uint8_t num, uint64_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_mid = (base >> 16) & 0xFFFF;
    idt[num].base_high = (base >> 32) & 0xFFFFFFFF;
    idt[num].selector = sel;
    idt[num].ist = 0;
    idt[num].flags = flags;
    idt[num].reserved = 0;
}

void idt_init() {
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint64_t)&idt;

    // Clear the IDT - set all entries to zero
    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_low = 0;
        idt[i].base_mid = 0;
        idt[i].base_high = 0;
        idt[i].selector = 0;
        idt[i].ist = 0;
        idt[i].flags = 0;
        idt[i].reserved = 0;
    }

    // Remap PIC (same as 32-bit)
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0x0);
    outb(0xA1, 0x0);

    // Set up PIT interrupt (IRQ0 = interrupt 32)
    // Code segment selector is 0x08 (from GDT)
    idt_set_gate(32, (uint64_t)irq0, 0x08, 0x8E);

    // Set up keyboard interrupt (IRQ1 = interrupt 33)
    idt_set_gate(33, (uint64_t)irq1, 0x08, 0x8E);

    idt_load((uint64_t)&idt_ptr);

    // Mask all IRQs except PIT (IRQ0) and keyboard (IRQ1)
    outb(0x21, 0xFC);  // Master PIC: 11111100
    outb(0xA1, 0xFF);  // Slave PIC: 11111111
}