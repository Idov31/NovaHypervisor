PUBLIC AsmInvept
PUBLIC AsmInvvpid

.code _text

VMX_ERROR_CODE_FAILED_WITH_STATUS   = 1
VMX_ERROR_CODE_FAILED               = 2

;
; Description:
; AsmInvept is responsible for invalidating the EPT cache.
;
; Parameters:
; @rcx		   [RegionType]   -- The region type to allocate.
; @rdx		   [INVEPT_DESC*] -- The address of the EPTP to invalidate.
;
; Returns:
; @invalidated [UCHAR]		  -- 0 if invalidated successfully, 1 if failed with status, 2 failed without status.
;
AsmInvept PROC PUBLIC
    invept  rcx, oword ptr [rdx]
    jz @ErrorWithStatus
    jc @ErrorNoStatus
    xor     rax, rax
    ret

    @ErrorWithStatus: 
    mov     rax, VMX_ERROR_CODE_FAILED_WITH_STATUS
    ret

    @ErrorNoStatus:
    mov     rax, VMX_ERROR_CODE_FAILED
    ret
AsmInvept ENDP

;
; Description:
; AsmInvvpid is responsible for invalidating virtual translation for a processor.
;
; Parameters:
; @rcx		   [RegionType]   -- The region type to allocate.
; @rdx		   [INVEPT_DESC*] -- The address of the EPTP to invalidate.
;
; Returns:
; @invalidated [UCHAR]		  -- 0 if invalidated successfully, 1 if failed with status, 2 failed without status.
;
AsmInvvpid PROC PUBLIC
    invvpid  rcx, oword ptr [rdx]
    jz @ErrorWithStatus
    jc @ErrorNoStatus
    xor     rax, rax
    ret

    @ErrorWithStatus: 
    mov     rax, VMX_ERROR_CODE_FAILED_WITH_STATUS
    ret

    @ErrorNoStatus:
    mov     rax, VMX_ERROR_CODE_FAILED
    ret
AsmInvvpid ENDP

END