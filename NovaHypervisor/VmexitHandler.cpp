#include "pch.h"
#include "VmexitHandler.h"

/*
* Description:
* VmexitHandler is responsible for handling the VMEXIT events.
*
* Parameters:
* @guestRegisters [_Inout_ PGUEST_REGISTERS] -- The guest registers.
*
* Returns:
* @bool										 -- The function returns true if vmxoff was executed, otherwise false.
*/
bool VmexitHandler(_Inout_ PGUEST_REGS guestRegisters) {
	SIZE_T exitReason = 0;
	SIZE_T exitQualification = 0;
	ULONG currentProcessorIndex = KeGetCurrentProcessorNumber();

	// Indicates we are in Vmx root mode in this logical core
	GuestState[currentProcessorIndex].IsOnVmxRoot = true;
	GuestState[currentProcessorIndex].IncrementRip = true;
	Ept* currentEptInstance = GuestState[currentProcessorIndex].EptInstance;

	__vmx_vmread(VM_EXIT_REASON, &exitReason);
	__vmx_vmread(EXIT_QUALIFICATION, &exitQualification);
	exitReason &= 0xffff;

	switch (exitReason) {
		case EXIT_REASON_EXCEPTION_NMI: {
			VMEXIT_INTERRUPT_INFO interruptExit = { 0 };
			__vmx_vmread(VM_EXIT_INTR_INFO, reinterpret_cast<SIZE_T*>(&interruptExit));

			if (interruptExit.InterruptionType == INTERRUPT_TYPE_SOFTWARE_EXCEPTION && interruptExit.Vector == EXCEPTION_VECTOR_BREAKPOINT) {
				GuestState[currentProcessorIndex].IncrementRip = false;
				EventHandler::InjectBreakpoint();
			}
			else if (interruptExit.InterruptionType == INTERRUPT_TYPE_HARDWARE_EXCEPTION && interruptExit.Vector == EXCEPTION_VECTOR_GENERAL_PROTECTION_FAULT) {
				GuestState[currentProcessorIndex].IncrementRip = false;
				EventHandler::InjectGeneralProtection();
			}
			else if (interruptExit.InterruptionType == INTERRUPT_TYPE_HARDWARE_EXCEPTION && interruptExit.Vector == EXCEPTION_VECTOR_UNDEFINED_OPCODE) {
				GuestState[currentProcessorIndex].IncrementRip = false;
				EventHandler::InjectUndefinedOpcode();
			}
			break;
		}
		case EXIT_REASON_TRIPLE_FAULT: {
			DbgBreakPoint();
			break;
		}

		case EXIT_REASON_VMCLEAR:
		case EXIT_REASON_VMPTRLD:
		case EXIT_REASON_VMPTRST:
		case EXIT_REASON_VMREAD:
		case EXIT_REASON_VMRESUME:
		case EXIT_REASON_VMWRITE:
		case EXIT_REASON_VMXOFF:
		case EXIT_REASON_VMXON:
		case EXIT_REASON_VMLAUNCH: {
			SIZE_T rflags = 0;
			__vmx_vmread(GUEST_RFLAGS, &rflags);
			__vmx_vmwrite(GUEST_RFLAGS, rflags | 0x1);
			break;
		}

		case EXIT_REASON_CR_ACCESS: {
			RegistersHandler::HandleCRAccess(guestRegisters);
			break;
		}
		case EXIT_REASON_MSR_READ:{
			RegistersHandler::HandleMSRRead(guestRegisters);
			break;
		}
		case EXIT_REASON_MSR_WRITE: {
			RegistersHandler::HandleMSRWrite(guestRegisters);
			break;
		}
		case EXIT_REASON_CPUID: {
			RegistersHandler::HandleCpuid(guestRegisters);
			break;
		}
		case EXIT_REASON_MONITOR_TRAP_FLAG: {
			if (GuestState[currentProcessorIndex].HookedPage) {
				currentEptInstance->HandleMonitorTrapFlag(GuestState[currentProcessorIndex].HookedPage);
				GuestState[currentProcessorIndex].HookedPage = NULL;
			}
			GuestState[currentProcessorIndex].IncrementRip = false;
			VmxHelper::SetMonitorTrapFlag(false);
			break;
		}
		case EXIT_REASON_EPT_VIOLATION: {
			SIZE_T physicalAddress = 0;
			__vmx_vmread(GUEST_PHYSICAL_ADDRESS, &physicalAddress);
			currentEptInstance->HandleEptViolation(exitQualification, physicalAddress);
			break;
		}
		case EXIT_REASON_EPT_MISCONFIG: {
			SIZE_T physicalAddress = 0;
			__vmx_vmread(GUEST_PHYSICAL_ADDRESS, &physicalAddress);
			currentEptInstance->HandleMisconfiguration(physicalAddress);
			break;
		}
		case EXIT_REASON_VMCALL: {
			if (IsSelfVmcall(guestRegisters->r10, guestRegisters->r11, guestRegisters->r12))
				guestRegisters->rax = VmcallHandler(guestRegisters->rcx, guestRegisters->rdx, guestRegisters->r8, guestRegisters->r9);
			else
				guestRegisters->rax = AsmHypervVmcall(guestRegisters->rcx, guestRegisters->rdx, guestRegisters->r8);
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_WARNING, "Unkown Vmexit, reason : 0x%llx", exitReason);
			break;
		}
	}

	if (!GuestState[currentProcessorIndex].VmxoffState.IsVmxoffExecuted && GuestState[currentProcessorIndex].IncrementRip)
		VmxHelper::ResumeToNextInstruction();

	// Set indicator of Vmx noon root mode to false
	GuestState[currentProcessorIndex].IsOnVmxRoot = false;
	return GuestState[currentProcessorIndex].VmxoffState.IsVmxoffExecuted;
}

/*
* Description:
* VmResumeInstruction is responsible for calling the VMRESUME instruction.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmResumeInstruction() {
	__vmx_vmresume();

	SIZE_T errorCode = 0;
	__vmx_vmread(VM_INSTRUCTION_ERROR, &errorCode);
	__vmx_off();
	NovaHypervisorLog(TRACE_FLAG_ERROR, "VM resume failed with error code: 0x%llx", errorCode);
}