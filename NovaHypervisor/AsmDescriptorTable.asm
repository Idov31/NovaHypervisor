PUBLIC AsmGetLdtr
PUBLIC AsmGetGdtBase
PUBLIC AsmGetIdtBase
PUBLIC AsmGetGdtLimit
PUBLIC AsmGetIdtLimit
PUBLIC AsmReloadGdtr
PUBLIC AsmReloadIdtr

.code _text

AsmGetLdtr PROC
	sldt	rax
	ret
AsmGetLdtr ENDP

AsmGetGdtBase PROC

	LOCAL	gdtr[10]:BYTE
	sgdt	gdtr
	mov		rax, QWORD PTR gdtr[2]
	ret
AsmGetGdtBase ENDP

AsmGetIdtBase PROC
	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		rax, QWORD PTR idtr[2]
	ret
AsmGetIdtBase ENDP

AsmGetGdtLimit PROC
	LOCAL	gdtr[10]:BYTE

	sgdt	gdtr
	mov		ax, WORD PTR gdtr[0]
	ret
AsmGetGdtLimit ENDP

AsmGetIdtLimit PROC
	LOCAL	idtr[10]:BYTE
	
	sidt	idtr
	mov		ax, WORD PTR idtr[0]
	ret
AsmGetIdtLimit ENDP

;
; Description:
; AsmReloadGdtr is responsible for reloading gdt.
;
; Parameters:
; @GdtBase	  [PVOID]  -- Gdt base.
; @GdtLimit	  [UINT32] -- Gdt limit.
;
; Returns:
; @gdt		  [PVOID]  -- New gdt.
;
AsmReloadGdtr PROC

    push	rcx
    shl		rdx, 48
    push	rdx
    lgdt	fword ptr [rsp+6]
    pop		rax
    pop		rax
    ret
    
AsmReloadGdtr ENDP

;
; Description:
; AsmReloadIdtr is responsible for reloading idt.
;
; Parameters:
; @IdtBase	  [PVOID]  -- Idt base.
; @IdtLimit	  [UINT32] -- Idt limit.
;
; Returns:
; @idt		  [PVOID]  -- New idt.
;

AsmReloadIdtr PROC
    
    push	rcx
    shl		rdx, 48
    push	rdx
    lidt	fword ptr [rsp+6]
    pop		rax
    pop		rax
    ret
    
AsmReloadIdtr ENDP

END