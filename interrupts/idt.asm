section .text
global idt_load
global irq0
global irq1
global irq_default
extern keyboard_handler
extern pit_handler

idt_load:
    lidt [rdi]          ; First argument in rdi (64-bit calling convention)
    ret

; Default handler for unhandled interrupts
irq_default:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov al, 0x20
    out 0x20, al

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq

; PIT interrupt handler (IRQ0)
irq0:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call pit_handler

    mov al, 0x20        ; EOI command
    out 0x20, al        ; Send to PIC

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq

; Keyboard interrupt handler (IRQ1)
irq1:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    call keyboard_handler

    mov al, 0x20        ; EOI command
    out 0x20, al        ; Send to PIC

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq               ; 64-bit interrupt return

global irq_page_fault
extern page_fault_handler

; Page fault handler (Exception 14)
irq_page_fault:
    ; Error code already pushed by CPU
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; Get faulting address from CR2
    mov rdi, cr2
    ; Get error code (it's at rsp + 15*8)
    mov rsi, [rsp + 120]

    call page_fault_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 8  ; Remove error code from stack
    iretq