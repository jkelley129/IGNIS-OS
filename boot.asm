  ; boot.asm
section .multiboot
align 8
multiboot_header:
    dd 0xE85250D6         ; magic
    dd 0
    dd multiboot_header_end - multiboot_header  ; Header length
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header)) ;Checksum

    ;End tag
    dw 0; Type
    dw 0; Flags
    dd 8; Size
multiboot_header_end:

section .bss
align 4096
p4_table:
    resb 4096
p3_table:
    resb 4096
p2_table:
    resb 4096
stack_bottom:
    resb 16384
stack_top:

section .text
bits 32
global _start
extern kernel_main

_start:
    mov esp, stack_top

    ; Save multiboot info
    mov edi, ebx

    ; Check for long mode support
    call check_long_mode

    ; Set up page tables
    call setup_page_tables

    ; Enable paging
    call enable_paging

    ; Load GDT
    lgdt [gdt64.pointer]

    ; Jump to 64-bit code
    jmp gdt64.code:long_mode_start

check_long_mode:
    ; Check for CPUID support
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    cmp eax, ecx
    je .no_long_mode

    ; Check for extended CPUID
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Check for long mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz .no_long_mode

    ret

.no_long_mode:
    hlt

setup_page_tables:
    ; Map P4[0] to P3
    mov eax, p3_table
    or eax, 0b11  ; present + writable
    mov [p4_table], eax

    ; Map P3[0] to P2
    mov eax, p2_table
    or eax, 0b11
    mov [p3_table], eax

    ; Map P2 entries (identity map first 2MB with huge pages)
    mov ecx, 0
.map_p2_table:
    mov eax, 0x200000  ; 2MB
    mul ecx
    or eax, 0b10000011  ; present + writable + huge page
    mov [p2_table + ecx * 8], eax
    inc ecx
    cmp ecx, 512
    jne .map_p2_table

    ret

enable_paging:
    ; Load P4 into CR3
    mov eax, p4_table
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable long mode in EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ret

bits 64
long_mode_start:
    ; Clear segment registers
    mov ax, gdt64.data
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Call kernel
    call kernel_main

    ; Hang if kernel returns
    hlt

section .rodata
gdt64:
    dq 0  ; null descriptor
.code: equ $ - gdt64
    dq (1<<41) | (1<<43) | (1<<44) | (1<<47) | (1<<53)  ; code segment: R (readable, bit 41), E (execute, bit 43), S (code/data, bit 44), P (present, bit 47), L (long mode, bit 53)
.data: equ $ - gdt64
    dq (1<<41) | (1<<44) | (1<<47)  ; data segment: W (writable, bit 41), S (code/data, bit 44), P (present, bit 47)
.pointer:
    dw $ - gdt64 - 1
    dq gdt64