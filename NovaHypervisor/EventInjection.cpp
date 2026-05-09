#include "pch.h"
#include "EventInjection.h"

namespace EventHandler {
	constexpr UINT32 EVENT_VALID_MASK = 1u << 31;
	constexpr UINT32 EVENT_ERROR_CODE_VALID_MASK = 1u << 11;
	constexpr UINT32 VM_ENTRY_EVENT_MASK = EVENT_VALID_MASK | EVENT_ERROR_CODE_VALID_MASK | 0x7ffu;

	/*
	* Description:
	* IsSoftwareEvent is responsible for checking whether an injected event consumes the VM-exit instruction length.
	*
	* Parameters:
	* @interruptionType [_In_ INTERRUPT_TYPE] -- The VMX interruption type.
	*
	* Returns:
	* @isSoftwareEvent [bool]				  -- True if the event is software-delivered.
	*/
	bool IsSoftwareEvent(_In_ INTERRUPT_TYPE interruptionType) {
		return interruptionType == INTERRUPT_TYPE_SOFTWARE_INTERRUPT ||
			interruptionType == INTERRUPT_TYPE_PRIVILEGED_SOFTWARE_INTERRUPT ||
			interruptionType == INTERRUPT_TYPE_SOFTWARE_EXCEPTION;
	}

	/*
	* Description:
	* ClearPendingInjection is responsible for clearing any pending event injection.
	* 
	* Parameters:
	* There are no parameters.
	* 
	* Returns:
	* There is no return value.
	*/
	void ClearPendingInjection() {
		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);
		__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
		__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, 0);
	}

	/*
	* Description:
	* SetupInstructionLength is responsible for setting up the VM-entry instruction length based on the VM-exit instruction length.
	* 
	* Parameters:
	* There are no parameters.
	* 
	* Returns:
	* There is no return value.
	*/
	void SetupInstructionLength() {
		SIZE_T exitInstractionLength = 0;
		__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &exitInstractionLength);
		__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, exitInstractionLength);
	}

	/*
	* Description:
	* ReinjectEventFromIdtVectoring is responsible for reinjecting an event from IDT vectoring information.
	* 
	* Parameters:
	* There are no parameters.
	* 
	* Returns:
	* @bool -- True if the event was reinjected, otherwise false.
	*/
	bool ReinjectEventFromIdtVectoring() {
		SIZE_T vmEntryInfo = 0;
		SIZE_T idtVectoringInfo = 0;
		__vmx_vmread(VM_ENTRY_INTR_INFO_FIELD, &vmEntryInfo);
		__vmx_vmread(IDT_VECTORING_INFO_FIELD, &idtVectoringInfo);

		if ((vmEntryInfo & EVENT_VALID_MASK) || !(idtVectoringInfo & EVENT_VALID_MASK))
			return false;

		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, idtVectoringInfo & VM_ENTRY_EVENT_MASK);

		if (idtVectoringInfo & EVENT_ERROR_CODE_VALID_MASK) {
			SIZE_T errorCode = 0;
			__vmx_vmread(IDT_VECTORING_ERROR_CODE, &errorCode);
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, errorCode);
		}

		const INTERRUPT_TYPE interruptionType =
			static_cast<INTERRUPT_TYPE>((idtVectoringInfo >> 8) & 0x7);
		if (IsSoftwareEvent(interruptionType))
			SetupInstructionLength();
		return true;
	}

	/*
	* Description
	* InjectEventFromVmExitInterruption is responsible for injecting an event from VM-exit interruption information.
	* 
	* Parameters:
	* @interruptExit [_In_ VMEXIT_INTERRUPT_INFO] -- The VM-exit interruption information.
	* 
	* Returns:
	* @bool										  -- True if the event was injected, otherwise false.
	*/
	bool InjectEventFromVmExitInterruption(_In_ VMEXIT_INTERRUPT_INFO interruptExit) {
		if (!interruptExit.Valid)
			return false;

		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, interruptExit.Flags & VM_ENTRY_EVENT_MASK);

		if (interruptExit.ErrorCodeValid) {
			SIZE_T errorCode = 0;
			__vmx_vmread(VM_EXIT_INTR_ERROR_CODE, &errorCode);
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, errorCode);
		}
		else {
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);
		}

		const INTERRUPT_TYPE interruptionType =
			static_cast<INTERRUPT_TYPE>(interruptExit.InterruptionType);
		if (IsSoftwareEvent(interruptionType))
			SetupInstructionLength();
		else
			__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, 0);
		return true;
	}

	/*
	* Description:
	* InjectInterruption is responsible for injecting an interruption to the guest.
	* 
	* Parameters:
	* @interruptionType		[_In_ INTERRUPT_TYPE] -- The VMX interruption type.
	* @vector				[_In_ EXCEPTION_VECTORS] -- The interruption vector to inject.
	* @deliverErrorCode		[_In_ bool]			  -- Indicates whether to deliver an error code with the interruption.
	* @errorCode			[_In_ ULONG32]		  -- The error code to deliver with the interruption, if applicable.
	* 
	* Returns:
	* There is no return value.
	*/
	void InjectInterruption(_In_ INTERRUPT_TYPE interruptionType, 
		_In_ EXCEPTION_VECTORS vector, 
		_In_ bool deliverErrorCode,
		_In_ ULONG32 errorCode) {

		INTERRUPT_INFO info = { 0 };
		info.Valid = TRUE;
		info.InterruptType = interruptionType;
		info.Vector = vector;
		info.DeliverCode = deliverErrorCode;
		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, info.Flags);

		if (deliverErrorCode)
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, errorCode);
		else
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, 0);

		if (!IsSoftwareEvent(interruptionType))
			__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, 0);
	}

	/*
	* Description:
	* InjectBreakpoint is responsible for injecting a breakpoint exception to the guest.
	* 
	* Parameters:
	* There are no parameters.
	* 
	* Returns:
	* There is no return value.
	*/
	void InjectBreakpoint() {
		InjectInterruption(INTERRUPT_TYPE_SOFTWARE_EXCEPTION, EXCEPTION_VECTOR_BREAKPOINT, false, 0);
		SetupInstructionLength();
	}

	/*
	* Description:
	* InjectGeneralProtection is responsible for injecting a general protection fault exception to the guest.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	void InjectGeneralProtection() {
		InjectInterruption(INTERRUPT_TYPE_HARDWARE_EXCEPTION, EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, true, 0);
	}

	/*
	* Description:
	* InjectUndefinedOpcode is responsible for injecting an undefined opcode exception to the guest.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	void InjectUndefinedOpcode() {
		InjectInterruption(INTERRUPT_TYPE_HARDWARE_EXCEPTION, EXCEPTION_VECTOR_UNDEFINED_OPCODE, false, 0);
	}

	/*
	* Description:
	* InjectPageFault is responsible for injecting a page fault exception to the guest with the provided faulting address and error code.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	void InjectPageFault(_In_ ULONG64 faultAddress, _In_ ULONG32 errorCode) {
		__writecr2(faultAddress);
		InjectInterruption(INTERRUPT_TYPE_HARDWARE_EXCEPTION, EXCEPTION_VECTOR_PAGE_FAULT, true, errorCode);
	}
};
