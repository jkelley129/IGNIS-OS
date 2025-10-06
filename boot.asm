  ; boot.asm
section .multiboot
align 4
multiboot_header:
    dd 0x1BADB002         ; magic
    dd 0x00000003         ; flags
    dd -(0x1BADB002 + 0x00000003) ; checksum

section .text
global _start
extern kernel_main

_start:
    mov esp, stack_space
    call kernel_main
    hlt

section .bss
resb 8192
stack_space:
