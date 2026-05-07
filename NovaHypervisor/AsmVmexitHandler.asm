PUBLIC AsmVmexitHandler

EXTERN VmexitHandler:PROC
EXTERN VmResumeFailure:PROC
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

    ; VMX does not save SIMD/FPU state on VM exits. Save the guest-visible
    ; x87/MMX/XMM state before calling C++ code, which may use volatile XMM
    ; registers directly or through compiler/runtime helpers.
    sub rsp, 0200h
    fxsave64 [rsp]

    ; Calling the VmexitHandler
    ; Subbing 0x20 from rsp for shadow space for the guest registers
	lea rcx, [rsp+0200h]
    mov rdx, rsp
	sub	rsp, 20h
	call	VmexitHandler
	add	rsp, 20h

    ; Check if we need to perform vmxoff
	cmp	al, 1
	je		AsmVmxoffHandler

    fxrstor64 [rsp]
    add rsp, 0200h

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

    vmresume

    ; VMRESUME returns only on failure. At this point the guest GPR/FPU state
    ; has already been restored, so do not attempt to resume again from C++.
    sub rsp, 028h
    call VmResumeFailure
    int 3
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
    ; RSP points at the saved FXSAVE frame on entry. The saved GPR frame starts
    ; at RSP+200h and guest state must not be restored until after the helper
    ; calls below are done.

    ; Getting the guest's rsp
    sub rsp, 020h
    call GetCurrentGuestRsp
    add rsp, 020h

    ; Getting the guest rip
    mov [rsp+0288h], rax
    sub rsp, 020h
    call GetCurrentGuestRip
    add rsp, 020h

    ; Loading the guest rsp and pushing the return address to the stack
    mov rdx, rsp
    mov rbx, [rsp+0288h]
    mov rsp, rbx
    push rax
    mov rsp, rdx

    ; Because there was a push, need to add 8 to the stack
    sub rbx,08h
    mov [rsp+0288h], rbx

    fxrstor64 [rsp]
    add rsp, 0200h

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
