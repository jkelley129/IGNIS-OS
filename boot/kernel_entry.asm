section .note.GNU-stack
bits 64
section .text

global kernel_entry
extern kernel_main

kernel_entry:
    ; Unmap identity mapping at P4[0]
    mov qword [0x1000], 0

    ; Flush TLB
    mov rax, cr3
    mov cr3, rax

    ; Setup higher-half stack
    extern stack_top
    lea rsp, [rel stack_top]

    ; Jump to C kernel
    call kernel_main

    ; Hang if kernel returns
    cli
.loop:
    hlt
    jmp .loop

; Stack in BSS
section .bss
align 16
stack_bottom:
    resb 16384
stack_top: