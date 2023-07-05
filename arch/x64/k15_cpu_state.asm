K15_COROUTINE_STATUS_DEAD EQU 2

system_offset_active_coroutine  EQU 0

coroutine_offset_fnc                EQU 0
coroutine_offset_fnc_args           EQU 8
coroutine_offset_stack              EQU 16
coroutine_offset_prev_stack         EQU 24
coroutine_offset_prev_cpu_state     EQU 32
coroutine_offset_cpu_state          EQU 132
coroutine_offset_status             EQU 232

coroutine_magic_number          EQU 15C0BEEFh

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

    movaps [rcx+8*10], xmm6
    movaps [rcx+8*14], xmm7
    movaps [rcx+8*18], xmm8
    movaps [rcx+8*22], xmm9
    movaps [rcx+8*26], xmm10
    movaps [rcx+8*30], xmm11
    movaps [rcx+8*34], xmm12
    movaps [rcx+8*38], xmm13
    movaps [rcx+8*42], xmm14
    movaps [rcx+8*46], xmm15
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

    movaps xmm6,  [rcx+8*10]
    movaps xmm7,  [rcx+8*14]
    movaps xmm8,  [rcx+8*18]
    movaps xmm9,  [rcx+8*22]
    movaps xmm10, [rcx+8*26]
    movaps xmm11, [rcx+8*30]
    movaps xmm12, [rcx+8*34]
    movaps xmm13, [rcx+8*38]
    movaps xmm14, [rcx+8*42]
    movaps xmm15, [rcx+8*46]

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

k15_mark_coroutine_as_dead PROC
    mov BYTE PTR [rcx], K15_COROUTINE_STATUS_DEAD   ; set the DEAD status to the coroutine (no offset needed, the status is the first member in the coroutine)
    ret
k15_mark_coroutine_as_dead ENDP

; Store the current cpu state to the global pMainThreadCpuState variable and call into the function of the coroutine
; Modifies the pMainThreadCpuState so that the RIP points to end_of_start_coroutine_asm when the 
; pMainThreadCpuState is applied again
k15_start_coroutine_asm PROC
    
    mov r12,    rcx
    mov r8,     [r12+coroutine_offset_prev_stack]           ; previous stack
    mov rcx,    r12
    add rcx,    coroutine_offset_prev_cpu_state         ; previous cpu state
    
    mov r9, end_of_start_coroutine              ; move address of end of function into r8
    mov r8, rsp
    mov [r8], r9                                ; write address of end of function into first member of pMainThreadCpuState (which is the rip)
                                                ; this ensures that the main thread continues at the end of this function after the coroutine yields or ends

    call k15_store_cpu_state
       
    mov rsp, [r12+coroutine_offset_stack]       ; set rsp to beginning of coroutine stack
    mov rcx, [r12+coroutine_offset_fnc_args]
    mov r9,  [r12+coroutine_offset_fnc]
    push r12                                    ; push pointer to coroutine to stack
    call r9                                     ; call coroutine function with coroutine argument

    ; NOTE:
    ; Control flow for the coroutine will only continue here once the coroutine function returns (and thus the coroutine is finished)
    ;
    ;

    mov rcx, r12
    call k15_mark_coroutine_as_dead             ; Mark coroutine as dead so that it's not getting run again in the future
    call k15_yield_asm                          ; Call yield last time to return code flow back to the main thread - coroutine system can now clean this coroutine up

end_of_start_coroutine:
    ret

k15_start_coroutine_asm ENDP

; Restores the cpu state of the coroutine whose address has been passed in as an argument
; This effectively resumes the control flow of the coroutine function where it last stopped
k15_continue_coroutine_asm PROC

    lea r10,    [rcx+coroutine_offset_cpu_state]               ; coroutine cpu state
    lea rcx,    [rcx+coroutine_offset_prev_cpu_state]

    call k15_store_cpu_state            ; store current cpu state into 
    
    mov r8, end_of_continue_coroutine   
    mov [rcx], r8                       ; patch pMainThreadCpuState.rip to contain the address of the end of this function

    mov rsp, [r10+coroutine_offset_stack]
    mov rcx, r10                        ; prepare argument for k15_apply_cpu_state
    call k15_apply_cpu_state            ; apply coroutine cpu state, effectively resuming control flow of the coroutine function

end_of_continue_coroutine:
    ret

k15_continue_coroutine_asm ENDP


; Stores the cpu state of the currently active coroutine by using the gloval pActiveCoroutine variable
k15_yield_asm PROC

    mov rcx, [rsp]                          ; get coroutine pointer from stack
    add rcx, coroutine_offset_cpu_state     ; add offset to coroutine to get cpuState into rcx
    call k15_store_cpu_state

    mov r8, end_of_yield            ; store address of end_of_yield label
    mov [rcx], r8                   ; set pActiveCoroutine.cpuState.rip to the address of the end_of_yield lable
                                    ; This is done so that when k15_apply_cpu_state is being called for the cpu state of the coroutine, it doesn't re-set the main thread cpu state
    
    mov rsp, [rcx+coroutine_offset_prev_stack]
    lea rcx, [rcx+coroutine_offset_prev_cpu_state]    ; Store address of pMainThreadCpuState into rcx
    call k15_apply_cpu_state                          ; Apply cpu state of the main thread, effectively returning control flow to the main thread
    
end_of_yield:
    ret

k15_yield_asm ENDP

END