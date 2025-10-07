/* link.ld - 64-bit version */
ENTRY(_start)

SECTIONS {
    . = 1M;

    .multiboot ALIGN(8) : {
        *(.multiboot)
    }

    .text ALIGN(4K) : {
        *(.text)
    }

    .rodata ALIGN(4K) : {
        *(.rodata)
    }

    .data ALIGN(4K) : {
        *(.data)
    }

    .bss ALIGN(4K) : {
        *(.bss)
        *(COMMON)
    }
}