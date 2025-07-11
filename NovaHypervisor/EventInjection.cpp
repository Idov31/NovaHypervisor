#include "pch.h"
#include "EventInjection.h"

namespace EventHandler {
	void SetupInstructionLength() {
		SIZE_T exitInstractionLength = 0;
		__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &exitInstractionLength);
		__vmx_vmwrite(VM_ENTRY_INSTRUCTION_LEN, exitInstractionLength);
	}

	void InjectInterruption(_In_ INTERRUPT_TYPE interruptionType, _In_ EXCEPTION_VECTORS vector, _In_ bool deliverErrorCode,
		_In_ ULONG32 errorCode) {

		INTERRUPT_INFO info = { 0 };
		info.Valid = TRUE;
		info.InterruptType = interruptionType;
		info.Vector = vector;
		info.DeliverCode = deliverErrorCode;
		__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, info.Flags);

		if (deliverErrorCode)
			__vmx_vmwrite(VM_ENTRY_EXCEPTION_ERROR_CODE, errorCode);
	}

	void InjectBreakpoint() {
		InjectInterruption(INTERRUPT_TYPE_SOFTWARE_INTERRUPT, EXCEPTION_VECTOR_BREAKPOINT, false, 0);
		SetupInstructionLength();
	}

	void InjectGeneralProtection() {
		InjectInterruption(INTERRUPT_TYPE_HARDWARE_EXCEPTION, EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT, true, 0);
		SetupInstructionLength();
	}

	void InjectUndefinedOpcode() {
		InjectInterruption(INTERRUPT_TYPE_HARDWARE_EXCEPTION, EXCEPTION_VECTOR_UNDEFINED_OPCODE, false, 0);
	}
};