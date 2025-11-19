section .note.GNU-stack
section .multiboot
align 8
multiboot_header:
    dd 0xE85250D6
    dd 0
    dd multiboot_header_end - multiboot_header
    dd -(0xE85250D6 + 0 + (multiboot_header_end - multiboot_header))
    dw 0
    dw 0
    dd 8
multiboot_header_end:

section .data
align 16
boot_gdt:
    dq 0
    dq 0x00209A0000000000  ; code
    dq 0x0000920000000000  ; data
boot_gdt_end:

boot_gdt_ptr:
    dw boot_gdt_end - boot_gdt - 1
    dd boot_gdt

align 4096
boot_p4:
    times 512 dq 0
boot_p3:
    times 512 dq 0
boot_p2:
    times 512 dq 0
boot_p3_high:
    times 512 dq 0
boot_p2_high:
    times 512 dq 0

; NEW: Direct map page tables
boot_p3_direct:
    times 512 dq 0
boot_p2_direct:
    times 512 dq 0

section .text
bits 32
global _start

_start:
    cli

    ; Stack below 1MB
    mov esp, 0x90000

    ; Save multiboot pointer
    mov edi, ebx

    ; Setup identity mapping and higher-half mapping
    ; P4[0] = P3 identity
    mov eax, boot_p3
    or eax, 3
    mov [boot_p4], eax

    ; P4[511] = P3 higher half
    mov eax, boot_p3_high
    or eax, 3
    mov [boot_p4 + 511*8], eax

    ; NEW: P4[256] = P3 direct map (0xFFFF800000000000)
    mov eax, boot_p3_direct
    or eax, 3
    mov [boot_p4 + 256*8], eax

    ; P3[0] = P2 identity
    mov eax, boot_p2
    or eax, 3
    mov [boot_p3], eax

    ; P3_high[510] = P2 higher half
    mov eax, boot_p2_high
    or eax, 3
    mov [boot_p3_high + 510*8], eax

    ; NEW: P3_direct[0] = P2 direct map
    mov eax, boot_p2_direct
    or eax, 3
    mov [boot_p3_direct], eax

    ; Map first 128MB identity with 2MB pages (64 pages)
    mov ecx, 0
.map_p2_low:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    mov [boot_p2 + ecx*8], eax
    inc ecx
    cmp ecx, 64
    jl .map_p2_low

    ; Map 0-128MB to higher half with 2MB pages (64 pages)
    mov ecx, 0
.map_p2_high:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    mov [boot_p2_high + ecx*8], eax
    inc ecx
    cmp ecx, 64
    jl .map_p2_high

    ; NEW: Map 0-128MB to direct map region with 2MB pages (64 pages)
    mov ecx, 0
.map_p2_direct:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    mov [boot_p2_direct + ecx*8], eax
    inc ecx
    cmp ecx, 64
    jl .map_p2_direct

    ; Load page table
    mov eax, boot_p4
    mov cr3, eax

    ; Enable PAE
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; Enable long mode
    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    ; Enable paging
    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    ; Load GDT
    lgdt [boot_gdt_ptr]

    ; Far jump to 64-bit code
    jmp 0x08:start64

bits 64
start64:
    ; Setup segments
    mov ax, 0x10
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Now jump to higher-half kernel
    extern kernel_entry
    jmp kernel_entry