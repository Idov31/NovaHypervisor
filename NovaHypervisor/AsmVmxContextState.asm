PUBLIC AsmVmxVmcall
PUBLIC AsmHypervVmcall
PUBLIC AsmVmxSaveState
PUBLIC AsmVmxRestoreState

EXTERN VirtualizeProcessor:PROC

.code _text

NO_HYPERV_MAGIC  = 4e4f485950455256H ; "NOHYPERV"
VMCALL_MAGIC	 = 564d43414c4cH	 ; "VMCALL"
HYPERVISOR_MAGIC = 4856544d50H		 ; "HVTMP"

;
; Description:
; AsmVmxVmcall is responsible for issueing a vmcall instruction.
;
; Parameters:
; @VmcallNumber	  [UINT64]	 -- Vmcall number
; @OptionalParam1 [UINT64]	 -- Parameter 1
; @OptionalParam2 [UINT64]	 -- Parameter 2
; @OptionalParam3 [UINT64]	 -- Parameter 3
;
; Returns:
; @status		  [NTSTATUS] -- STATUS_SUCCESS if the vmcall was successful, otherwise an error code.
;
AsmVmxVmcall PROC
	pushfq
    push    r10
    push    r11
    push    r12
    mov     r10, HYPERVISOR_MAGIC
    mov     r11, VMCALL_MAGIC
    mov     r12, NO_HYPERV_MAGIC
    vmcall
    pop     r12
    pop     r11
    pop     r10
    popfq
    ret    
AsmVmxVmcall ENDP

;
; Description:
; AsmHypervVmcall is responsible for issueing a vmcall to hyperv.
;
; Parameters:
; @HypercallNumber [UINT64]	 -- Hypercall number
; @InputParam      [UINT64]	 -- The guest physical address of the input parameter.
; @OutputParam     [UINT64]	 -- The guest physical address of the output parameter.
;
; Returns:
; @status		  [NTSTATUS] -- STATUS_SUCCESS if the vmcall was successful, otherwise an error code.
;
AsmHypervVmcall PROC
    ; Saving the registers
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
    push rbp	; rsp
    push rbx
    push rdx
    push rcx
    push rax

    ; Save the FXSAVE frame pointer passed in RDX. Hyper-V may have exposed
    ; XMM fast hypercalls before Nova was loaded, so CPUID masking alone is not
    ; enough: forward XMM0-XMM5 from the guest frame and capture any fast-output
    ; values back into that same frame.
    push rdx

    movdqu xmm0, xmmword ptr [rdx+0a0h]
    movdqu xmm1, xmmword ptr [rdx+0b0h]
    movdqu xmm2, xmmword ptr [rdx+0c0h]
    movdqu xmm3, xmmword ptr [rdx+0d0h]
    movdqu xmm4, xmmword ptr [rdx+0e0h]
    movdqu xmm5, xmmword ptr [rdx+0f0h]

    ; Unpacking the registers from rcx
    mov rax, qword ptr [rcx+0h]
    mov rdx, qword ptr [rcx+10h]
    mov rbx, qword ptr [rcx+18h]
    mov rbp, qword ptr [rcx+28h]
    mov rsi, qword ptr [rcx+30h]
    mov rdi, qword ptr [rcx+38h]
    mov r8, qword ptr [rcx+40h]
    mov r9, qword ptr [rcx+48h]
    mov r10, qword ptr [rcx+50h]
    mov r11, qword ptr [rcx+58h]
    mov r12, qword ptr [rcx+60h]
    mov r13, qword ptr [rcx+68h]
    mov r14, qword ptr [rcx+70h]
    mov r15, qword ptr [rcx+78h]

    ; Saving the packed rcx
    push rcx
    mov rcx, qword ptr [rcx+08h]

    vmcall

    ; Save Hyper-V's returned RAX/RCX before recovering local pointers.
    push rax
    push rcx
    mov rcx, qword ptr [rsp+10h]

    mov rax, qword ptr [rsp+18h]
    movdqu xmmword ptr [rax+0a0h], xmm0
    movdqu xmmword ptr [rax+0b0h], xmm1
    movdqu xmmword ptr [rax+0c0h], xmm2
    movdqu xmmword ptr [rax+0d0h], xmm3
    movdqu xmmword ptr [rax+0e0h], xmm4
    movdqu xmmword ptr [rax+0f0h], xmm5

    ; Copy the guest-visible post-hypercall state back to the saved VM-exit frame.
    ; Hyper-V guarantees untouched registers remain unchanged, so copying the full
    ; GPR set preserves rep hypercall RCX, fast-output RDX/R8, and special cases
    ; such as HvCallSetVpRegisters without relying on per-call decoding here.
    mov rax, qword ptr [rsp+08h]
    mov qword ptr [rcx+0h], rax
    mov rax, qword ptr [rsp]
    mov qword ptr [rcx+08h], rax
    mov qword ptr [rcx+10h], rdx
    mov qword ptr [rcx+18h], rbx
    mov qword ptr [rcx+28h], rbp
    mov qword ptr [rcx+30h], rsi
    mov qword ptr [rcx+38h], rdi
    mov qword ptr [rcx+40h], r8
    mov qword ptr [rcx+48h], r9
    mov qword ptr [rcx+50h], r10
    mov qword ptr [rcx+58h], r11
    mov qword ptr [rcx+60h], r12
    mov qword ptr [rcx+68h], r13
    mov qword ptr [rcx+70h], r14
    mov qword ptr [rcx+78h], r15

    add rsp, 10h
    pop rcx
    add rsp, 08h

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
    ret
AsmHypervVmcall ENDP

;
; Description:
; AsmVmxSaveState is responsible for saving the current state and launching the vm.
;
; Parameters:
; There are no parameters.
;
; Returns:
; There is no return value.
;
AsmVmxSaveState PROC
	pushfq	; save r/eflag

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

	sub rsp, 0100h
	mov rcx, rsp

    ; Reserve Windows x64 shadow space and align the stack for the C++ helper
    ; without moving the guest stack pointer passed in RCX.
	sub rsp, 028h
	call VirtualizeProcessor
	add rsp, 028h

	; Shouldn't be reached, added for fail safe.
	int 3
	jmp AsmVmxRestoreState
AsmVmxSaveState ENDP

;
; Description:
; AsmVmxRestoreState is responsible for restoring the state.
;
; Parameters:
; There are no parameters.
;
; Returns:
; There is no return value.
;
AsmVmxRestoreState PROC
	add rsp, 0100h

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
	popfq

	ret	
AsmVmxRestoreState ENDP

END
