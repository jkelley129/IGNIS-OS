section .text
global task_switch

; void task_switch(cpu_state_t** old_context, cpu_state_t* new_context)
; rdi = pointer to pointer to old context (we'll update it)
; rsi = pointer to new context (we'll load from it)
task_switch:
    ; === SAVE OLD CONTEXT ===

    ; Push all general purpose registers (matching cpu_state_t order)
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8
    push rbp
    push rdi
    push rsi
    push rdx
    push rcx
    push rbx
    push rax

    ; Push RIP (return address - where we'll come back to)
    ; The return address is currently at [rsp + 120] (15 registers * 8 bytes)
    lea rax, [rsp + 120]
    push qword [rax]

    ; Push CS (code segment)
    mov ax, cs
    push rax

    ; Push RFLAGS
    pushfq

    ; Push RSP (current stack pointer value)
    push rsp

    ; Push SS (stack segment)
    mov ax, ss
    push rax

    ; Save current RSP to old context
    ; [rdi] = rsp (save stack pointer to old_context)
    mov [rdi], rsp

    ; === LOAD NEW CONTEXT ===

    ; Switch to new stack
    mov rsp, rsi

    ; Pop SS (stack segment) - but we don't actually load it
    add rsp, 8

    ; Pop RSP - but we don't actually load it (we already switched)
    add rsp, 8

    ; Pop RFLAGS
    popfq

    ; Pop CS - but we don't actually load it (kernel stays in kernel)
    add rsp, 8

    ; Pop RIP into RAX (we'll use it later)
    pop rax

    ; Pop all general purpose registers (in reverse order)
    pop rax     ; This will be overwritten, but that's OK
    pop rbx
    pop rcx
    pop rdx
    pop rsi
    pop rdi
    pop rbp
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15

    ; Now jump to the saved RIP
    ; Get RIP from the stack where we saved it
    mov rax, [rsp - 120]  ; Get saved RIP
    jmp rax