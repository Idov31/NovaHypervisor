PUBLIC AsmVmexitHandler

EXTERN VmexitHandler:PROC
EXTERN VmResumeFailure:PROC
EXTERN GetCurrentGuestRsp:PROC
EXTERN GetCurrentGuestRip:PROC

.code _text

XSTATE_SAVE_AREA_SIZE      EQU 03000h
XSTATE_STACK_FRAME_SIZE    EQU 03100h
XSTATE_GPR_FRAME_OFFSET    EQU 03000h
XSTATE_SAVE_KIND_OFFSET    EQU 03008h
XSTATE_XCR0_LOW_OFFSET     EQU 03010h
XSTATE_XCR0_HIGH_OFFSET    EQU 03018h
CR4_OSXSAVE_MASK           EQU 040000h

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

    ; VMX does not save SIMD/FPU state on VM exits. Reserve a fixed aligned
    ; frame for either XSAVE/XRSTOR or the legacy FXSAVE/FXRSTOR fallback.
    mov r11, rsp
    sub rsp, XSTATE_STACK_FRAME_SIZE
    and rsp, 0FFFFFFFFFFFFFFC0h
    mov qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET], r11
    mov qword ptr [rsp+XSTATE_SAVE_KIND_OFFSET], 0

    mov rax, cr4
    test rax, CR4_OSXSAVE_MASK
    jz SaveLegacyFxState

    xor ecx, ecx
    xgetbv
    mov dword ptr [rsp+XSTATE_XCR0_LOW_OFFSET], eax
    mov dword ptr [rsp+XSTATE_XCR0_HIGH_OFFSET], edx
    xsave64 [rsp]
    mov qword ptr [rsp+XSTATE_SAVE_KIND_OFFSET], 1
    jmp GuestFxStateSaved

SaveLegacyFxState:
    fxsave64 [rsp]

GuestFxStateSaved:
    ; Calling the VmexitHandler
    ; Subbing 0x20 from rsp for shadow space for the guest registers
	mov rcx, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]
    mov rdx, rsp
	sub	rsp, 20h
	call	VmexitHandler
	add	rsp, 20h

    ; Check if we need to perform vmxoff
	cmp	al, 1
	je		AsmVmxoffHandler

    cmp qword ptr [rsp+XSTATE_SAVE_KIND_OFFSET], 1
    jne RestoreLegacyFxState

    mov eax, dword ptr [rsp+XSTATE_XCR0_LOW_OFFSET]
    mov edx, dword ptr [rsp+XSTATE_XCR0_HIGH_OFFSET]
    xrstor64 [rsp]
    jmp GuestFxStateRestored

RestoreLegacyFxState:
    fxrstor64 [rsp]

GuestFxStateRestored:
    mov rsp, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]

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
    ; RSP points at the saved XSAVE/FXSAVE frame on entry. The saved GPR frame
    ; pointer is stored in the private metadata area and guest state must not be
    ; restored until after the helper calls below are done.

    ; Getting the guest's rsp
    sub rsp, 020h
    call GetCurrentGuestRsp
    add rsp, 020h

    ; Getting the guest rip
    mov r10, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]
    mov [r10+088h], rax
    sub rsp, 020h
    call GetCurrentGuestRip
    add rsp, 020h

    ; Loading the guest rsp and pushing the return address to the stack
    mov rdx, rsp
    mov r10, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]
    mov rbx, [r10+088h]
    mov rsp, rbx
    push rax
    mov rsp, rdx

    ; Because there was a push, need to add 8 to the stack
    sub rbx,08h
    mov r10, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]
    mov [r10+088h], rbx

    cmp qword ptr [rsp+XSTATE_SAVE_KIND_OFFSET], 1
    jne VmxoffRestoreLegacyFxState

    mov eax, dword ptr [rsp+XSTATE_XCR0_LOW_OFFSET]
    mov edx, dword ptr [rsp+XSTATE_XCR0_HIGH_OFFSET]
    xrstor64 [rsp]
    jmp VmxoffGuestFxStateRestored

VmxoffRestoreLegacyFxState:
    fxrstor64 [rsp]

VmxoffGuestFxStateRestored:
    mov rsp, qword ptr [rsp+XSTATE_GPR_FRAME_OFFSET]

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
