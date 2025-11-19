section .note.GNU-stack
section .text
global task_switch

; void task_switch(cpu_state_t** old_context, cpu_state_t* new_context)
; rdi = pointer to pointer to old context (where to save current RSP)
; rsi = pointer to new context (stack to restore)
task_switch:
    ; === SAVE OLD CONTEXT ===
    ; Save all callee-saved registers (System V AMD64 ABI)
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Save current stack pointer to old_context
    mov [rdi], rsp

    ; === LOAD NEW CONTEXT ===
    ; Switch to new stack
    mov rsp, rsi

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return to wherever this task was (or to task entry point for new tasks)
    ret