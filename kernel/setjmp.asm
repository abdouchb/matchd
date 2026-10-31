global setjmp
global longjmp

section .text
bits 64

; System V AMD64 ABI: 
; 1st argument (env) is in RDI
; 2nd argument (val) is in RSI

; int setjmp(jmp_buf env)
setjmp:
    ; Save RBX, RBP, R12-R15
    mov [rdi + 0x00], rbx
    mov [rdi + 0x08], rbp
    mov [rdi + 0x10], r12
    mov [rdi + 0x18], r13
    mov [rdi + 0x20], r14
    mov [rdi + 0x28], r15

    ; Save RSP before the call (caller's stack pointer)
    lea rax, [rsp + 8]
    mov [rdi + 0x30], rax

    ; Save RIP (return address)
    mov rax, [rsp]
    mov [rdi + 0x38], rax

    ; Return 0
    xor eax, eax
    ret

; void longjmp(jmp_buf env, int val)
longjmp:
    ; Set return value (EAX) to val. If val is 0, return 1.
    mov eax, esi
    test eax, eax
    jnz .restore
    inc eax
.restore:
    ; Restore RBX, RBP, R12-R15
    mov rbx, [rdi + 0x00]
    mov rbp, [rdi + 0x08]
    mov r12, [rdi + 0x10]
    mov r13, [rdi + 0x18]
    mov r14, [rdi + 0x20]
    mov r15, [rdi + 0x28]
    
    ; Restore RSP
    mov rsp, [rdi + 0x30]
    
    ; Restore RIP and jump back
    mov rcx, [rdi + 0x38]
    jmp rcx
