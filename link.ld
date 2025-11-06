/* link.ld */
ENTRY(_start)

KERNEL_VIRT_BASE = 0xFFFFFFFF80000000;
KERNEL_PHYS_BASE = 0x100000;

SECTIONS {
    /* Boot code stays at low physical addresses */
    . = KERNEL_PHYS_BASE;

    .multiboot ALIGN(8) : AT(ADDR(.multiboot)) {
        *(.multiboot)
    }

    .boot_data ALIGN(16) : AT(ADDR(.boot_data)) {
        build/asm_boot.o(.data)
        build/asm_boot.o(.bss)
    }

    .boot_text ALIGN(4K) : AT(ADDR(.boot_text)) {
        build/asm_boot.o(.text)
    }

    /* Kernel in higher half */
    . += KERNEL_VIRT_BASE;

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VIRT_BASE) {
        build/asm_kernel_entry.o(.text)
        *(.text)
    }

    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VIRT_BASE) {
        *(.rodata)
    }

    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VIRT_BASE) {
        *(.data)
    }

    .bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VIRT_BASE) {
        *(.bss)
        *(COMMON)
    }
}