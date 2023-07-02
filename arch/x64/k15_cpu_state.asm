EXTERN pActiveCoroutine:PTR
EXTERN pMainThreadStackPtr:PTR
EXTERN pMainThreadCpuState:PTR

.CODE

; This function stores the current state of all non-volatile registers to a buffer passed as argument
k15_store_cpu_state PROC

    mov r8, [rsp]
    mov [rcx+8*0], r8
    mov [rcx+8*1], rbx
    mov [rcx+8*2], rbp
    mov [rcx+8*3], rdi
    mov [rcx+8*4], rsi
    mov [rcx+8*5], rsp
    mov [rcx+8*6], r12
    mov [rcx+8*7], r13
    mov [rcx+8*8], r14
    mov [rcx+8*9], r15
    ret

k15_store_cpu_state ENDP

; This function applies the register state passed as a buffer as an argument to all non-volatile registers
; Additionally, the stack is modified so that the RIP points to the next instruction when k15_store_cpu_state was called
k15_apply_cpu_state PROC

    mov r8,  [rcx+8*0]
    mov rbx, [rcx+8*1]
    mov rbp, [rcx+8*2]
    mov rdi, [rcx+8*3]
    mov rsi, [rcx+8*4]
    mov rsp, [rcx+8*5]
    mov r12, [rcx+8*6]
    mov r13, [rcx+8*7]
    mov r14, [rcx+8*8]
    mov r15, [rcx+8*9]
    pop r9              ;pop current return address
    push r8             ;push new return address from cpu state
    ret

k15_apply_cpu_state ENDP

; This function returns the value of RSP
k15_get_stack_pointer PROC

    mov rax, rsp
    ret

k15_get_stack_pointer ENDP

; Sets the address passed as argument to the current RSP
k15_set_stack_pointer PROC

    mov r8, [rsp]
    mov rsp, rcx
    push r8
    ret

k15_set_stack_pointer ENDP

; Store the current cpu state to the global pMainThreadCpuState variable and call into the function of the coroutine
; Modifies the pMainThreadCpuState so that the RIP points to end_of_start_coroutine_asm when the 
; pMainThreadCpuState is applied again
k15_start_coroutine_asm PROC

    mov r9,     [rcx+8*2]                       ; coroutine function
    mov r10,    [rcx+8*3]                       ; coroutine function arguments
    mov r11,    [rcx+8*4]                       ; coroutine stack

    mov rcx,    pMainThreadCpuState 
    call k15_store_cpu_state                    ; store main thread cpu state into pMainThreadCpuState

    
    mov r8, end_of_start_coroutine              ; move address of end of function into r8
    mov rdx, pMainThreadCpuState                ; move address of pMainThreadCpuState into rdx
    mov [rdx], r8                               ; write address of end of function into first member of pMainThreadCpuState (which is the rip)
                                                ; this ensures that the main thread continues at the end of this function after the coroutine yields or ends

    mov pMainThreadStackPtr, rsp                ; store current rsp into pMainThreadStackPtr

    mov rsp, r11                                ; set rsp to beginning of coroutine stack
    mov rcx, r10

    call r9                                     ; call coroutine function with coroutine argument

    ; NOTE:
    ; Control flow for the coroutine will only continue here once the coroutine function returns (and thus the coroutine is finished)
    ;
    ;
    mov r9, pActiveCoroutine
    mov r10, 0h
    mov [r9], r10                               ; remove a flag in the currently active coroutine to signal that the coroutine is finished
    call k15_yield_asm                     ; Call yield last time to return code flow back to the main thread - coroutine system can now clean this coroutine up

end_of_start_coroutine:
    mov rsp, pMainThreadStackPtr                ; set stack pointer to previously stored rsp
    ret

k15_start_coroutine_asm ENDP

; Restores the cpu state of the coroutine whose address has been passed in as an argument
; This effectively resumes the control flow of the coroutine function where it last stopped
k15_continue_coroutine_asm PROC

    lea r10,    [rcx+8*5]               ; coroutine cpu state

    mov pMainThreadStackPtr, rsp        ; store current RSP into pMainThreadStackPtr
    mov rcx, pMainThreadCpuState        ; prepare argument for k15_store_cpu_state
    call k15_store_cpu_state            ; store current cpu state into pMainThreadCpuState
    
    mov r8, end_of_continue_coroutine   
    mov [rcx], r8                       ; patch pMainThreadCpuState.rip to contain the address of the end of this function

    mov rsp,    [r10+8*5]

    mov rcx, r10                        ; prepare argument for k15_apply_cpu_state
    call k15_apply_cpu_state            ; apply coroutine cpu state, effectively resuming control flow of the coroutine function

end_of_continue_coroutine:
    ret

k15_continue_coroutine_asm ENDP


; Stores the cpu state of the currently active coroutine by using the gloval pActiveCoroutine variable
k15_yield_asm PROC

    mov rcx, pActiveCoroutine       ; store address of pActiveCoroutine into rcx
    add rcx, 8*5                    ; add offset to pActiveCoroutine.cpuState
    call k15_store_cpu_state

    mov r8, end_of_yield            ; store address of end_of_yield label
    mov [rcx], r8                   ; set pActiveCoroutine.cpuState.rip to the address of the end_of_yield lable
                                    ; This is done so that when k15_apply_cpu_state is being called for the cpu state of the coroutine, it doesn't re-set the main thread cpu state
    
    mov rsp, pMainThreadStackPtr    ; Restore main thread stack by setting rsp to pMainThreadStackPtr
    mov rcx, pMainThreadCpuState    ; Store address of pMainThreadCpuState into rcx
    call k15_apply_cpu_state        ; Apply cpu state of the main thread, effectively returning control flow to the main thread
    
end_of_yield:
    ret

k15_yield_asm ENDP

END