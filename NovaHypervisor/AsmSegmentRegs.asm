PUBLIC AsmGetCs
PUBLIC AsmGetDs
PUBLIC AsmGetEs
PUBLIC AsmGetSs
PUBLIC AsmGetFs
PUBLIC AsmGetGs
PUBLIC AsmGetTr
PUBLIC AsmGetRflags

.code _text

AsmGetRflags PROC
	pushfq
	pop		rax
	ret
AsmGetRflags ENDP

AsmGetCs PROC
	mov		rax, cs
	ret
AsmGetCs ENDP

AsmGetDs PROC
	mov		rax, ds
	ret
AsmGetDs ENDP

AsmGetEs PROC
	mov		rax, es
	ret
AsmGetEs ENDP

AsmGetSs PROC
	mov		rax, ss
	ret
AsmGetSs ENDP

AsmGetFs PROC
	mov		rax, fs
	ret
AsmGetFs ENDP

AsmGetGs PROC
	mov		rax, gs
	ret
AsmGetGs ENDP

AsmGetTr PROC
	str	rax
	ret
AsmGetTr ENDP

END