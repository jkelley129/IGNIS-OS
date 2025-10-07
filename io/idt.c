#include "idt.h"
#include "ports.h"

// External assembly functions
extern void idt_load(uint32_t);
extern void irq1();
extern void irq_default();

static idt_entry_t idt[IDT_ENTRIES];
static idt_ptr_t idt_ptr;

void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].flags = flags;
}

void idt_init() {
    idt_ptr.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
    idt_ptr.base = (uint32_t)&idt;

    // Clear the IDT - set all entries to zero
    for (uint16_t i = 0; i < IDT_ENTRIES; i++) {
        idt[i].base_low = 0;
        idt[i].base_high = 0;
        idt[i].selector = 0;
        idt[i].zero = 0;
        idt[i].flags = 0;
    }

    // Remap PIC
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

    // Set up keyboard interrupt (IRQ1 = interrupt 33)
    idt_set_gate(33, (uint32_t)irq1, 0x08, 0x8E);

    idt_load((uint32_t)&idt_ptr);

    // Mask all IRQs except keyboard (IRQ1)
    // This MUST come after idt_load or interrupts might fire before IDT is ready
    outb(0x21, 0xFD);  // Master PIC: 11111101 (all masked except IRQ1)
    outb(0xA1, 0xFF);  // Slave PIC: 11111111 (all masked)
}