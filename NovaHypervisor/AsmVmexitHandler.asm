PUBLIC AsmVmexitHandler

EXTERN VmexitHandler:PROC
EXTERN VmResumeInstruction:PROC
EXTERN GetCurrentGuestRsp:PROC
EXTERN GetCurrentGuestRip:PROC

.code _text

;
; Description:
; AsmVmexitHandler is responsible for handling vmexit
;
; Parameters:
; There are no parameters.
;
; Returns:
; There is no return value.
;
AsmVmexitHandler PROC
    ; Stack alignment
    push 0

    pushfq
    push r15
    push r14
    push r13
    push r12
    push r11
    push r10
    push r9
    push r8        
    push rdi
    push rsi
    push rbp
    push rbp ; rsp
    push rbx
    push rdx
    push rcx
    push rax	

    ; Calling the VmexitHandler
    ; Subbing 0x28 from rsp for shadow space for the guest registers
	mov rcx, rsp
	sub	rsp, 28h
	call	VmexitHandler
	add	rsp, 28h

    ; Check if we need to perform vmxoff
	cmp	al, 1
	je		AsmVmxoffHandler

    ; Restore the guest state
	pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp		; rsp
    pop rbp
    pop rsi
    pop rdi 
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    popfq

	sub rsp, 0100h
	jmp VmResumeInstruction
AsmVmexitHandler ENDP

;
; Description:
; AsmVmxoffHandler is responsible for handling vmxoff and return to the guest
;
; Parameters:
; There are no parameters.
;
; Returns:
; There is no return value.
;
AsmVmxoffHandler PROC
    ; Getting the guest's rsp
    sub rsp, 020h
    call GetCurrentGuestRsp
    add rsp, 020h

    ; Getting the guest rip
    mov [rsp+088h], rax
    sub rsp, 020h
    call GetCurrentGuestRip
    add rsp, 020h

    ; Loading the guest rsp and pushing the return address to the stack
    mov rdx, rsp
    mov rbx, [rsp+088h]
    mov rsp, rbx
    push rax
    mov rsp, rdx

    ; Because there was a push, need to add 8 to the stack
    sub rbx,08h
    mov [rsp+088h], rbx

	pop rax
    pop rcx
    pop rdx
    pop rbx
    pop rbp ; rsp
    pop rbp
    pop rsi
    pop rdi 
    pop r8
    pop r9
    pop r10
    pop r11
    pop r12
    pop r13
    pop r14
    pop r15
    popfq

	pop rsp
	ret
AsmVmxoffHandler ENDP

END
