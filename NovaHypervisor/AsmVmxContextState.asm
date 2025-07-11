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

    ; Restoring the state
    pop rcx

    mov qword ptr [rcx+0h], rax
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
    mov qword ptr [rcx+08h], rcx

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

	call VirtualizeProcessor

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
