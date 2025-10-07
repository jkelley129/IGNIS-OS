section .text
global idt_load
global irq1
global irq_default
extern keyboard_handler

idt_load:
    mov eax, [esp+4]    ; Get IDT pointer address
    lidt [eax]          ; Load IDT
    ; DON'T enable interrupts yet - we'll do it manually in C
    ret

; Default handler for unhandled interrupts
irq_default:
    pushad
    mov al, 0x20
    out 0x20, al
    popad
    iret

; Keyboard interrupt handler (IRQ1)
irq1:
    pushad              ; Push all registers (32-bit version)

    call keyboard_handler

    mov al, 0x20        ; EOI command
    out 0x20, al        ; Send to PIC

    popad               ; Pop all registers
    iret                ; Return from interrupt