#include "pch.h"
#include "VmexitHandler.h"

/*
* Description:
* LogUnhandledVmExit is responsible for recording enough guest state to debug an unexpected VM-exit.
*
* Parameters:
* @exitReason		  [_In_ SIZE_T] -- The VM-exit reason.
* @exitQualification [_In_ SIZE_T] -- The VM-exit qualification field.
*
* Returns:
* There is no return value.
*/
static void LogUnhandledVmExit(_In_ SIZE_T exitReason, _In_ SIZE_T exitQualification) {
	SIZE_T guestRip = 0;
	SIZE_T guestRsp = 0;
	SIZE_T guestCr3 = 0;
	SIZE_T guestRflags = 0;
	__vmx_vmread(GUEST_RIP, &guestRip);
	__vmx_vmread(GUEST_RSP, &guestRsp);
	__vmx_vmread(GUEST_CR3, &guestCr3);
	__vmx_vmread(GUEST_RFLAGS, &guestRflags);

	NovaHypervisorLog(TRACE_FLAG_ERROR,
		"Unhandled VM-exit reason=0x%llx qualification=0x%llx RIP=0x%llx RSP=0x%llx CR3=0x%llx RFLAGS=0x%llx",
		exitReason,
		exitQualification,
		guestRip,
		guestRsp,
		guestCr3,
		guestRflags);
}

/*
* Description:
* EmulateXsetbv is responsible for validating and applying guest XCR0 updates.
*
* Parameters:
* @guestRegisters [_In_ PGUEST_REGS] -- The guest register frame containing XSETBV operands.
*
* Returns:
* @emulated	   [bool]			   -- True if the XSETBV operation was accepted.
*/
static bool EmulateXsetbv(_In_ PGUEST_REGS guestRegisters) {
	if (guestRegisters->rcx != 0)
		return false;

	int cpuInfo[4] = { 0 };
	__cpuidex(cpuInfo, static_cast<int>(CPUID_EXTENDED_STATE_ENUMERATION), 0);

	const ULONG64 supportedMask =
		(static_cast<ULONG64>(static_cast<ULONG>(cpuInfo[3])) << 32) |
		static_cast<ULONG>(cpuInfo[0]);
	const ULONG64 requestedMask =
		(guestRegisters->rdx << 32) |
		(guestRegisters->rax & 0xFFFFFFFF);

	if ((requestedMask & ~supportedMask) ||
		!(requestedMask & 0x1) ||
		((requestedMask & 0x4) && ((requestedMask & 0x6) != 0x6))) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Invalid guest XSETBV value: 0x%llx supported=0x%llx", requestedMask, supportedMask);
		return false;
	}

	ULONG requestedSize = 0x240;

	for (ULONG stateComponent = 2; stateComponent < 64; stateComponent++) {
		if (!(requestedMask & (1ULL << stateComponent)))
			continue;

		int stateInfo[4] = { 0 };
		__cpuidex(stateInfo, static_cast<int>(CPUID_EXTENDED_STATE_ENUMERATION), static_cast<int>(stateComponent));

		const ULONG componentSize = static_cast<ULONG>(stateInfo[0]);
		const ULONG componentOffset = static_cast<ULONG>(stateInfo[1]);

		if (componentSize == 0 || componentOffset == 0)
			continue;

		const ULONG componentEnd = componentOffset + componentSize;

		if (componentEnd > requestedSize)
			requestedSize = componentEnd;
	}

	if (requestedSize > MAX_XSAVE_AREA_SIZE) {
		NovaHypervisorLog(TRACE_FLAG_ERROR,
			"Guest XSETBV requires unsupported XSAVE area size: XCR0=0x%llx required=0x%x max=0x%llx",
			requestedMask,
			requestedSize,
			MAX_XSAVE_AREA_SIZE);
		return false;
	}

	_xsetbv(static_cast<ULONG>(guestRegisters->rcx), requestedMask);
	return true;
}

/*
* Description:
* VmexitHandler is responsible for handling the VMEXIT events.
*
* Parameters:
* @guestRegisters [_Inout_ PGUEST_REGISTERS] -- The guest registers.
* @guestFxState   [_In_ UINT64]				 -- The guest FPU state.
*
* Returns:
* @bool										 -- The function returns true if vmxoff was executed, otherwise false.
*/
bool VmexitHandler(_Inout_ PGUEST_REGS guestRegisters, _In_ UINT64 guestFxState) {
	SIZE_T exitReason = 0;
	SIZE_T exitQualification = 0;
	ULONG currentProcessorIndex = KeGetCurrentProcessorNumber();
	VmExitAction action = VmExitAction::ResumeCurrentRip;

	// Indicates we are in Vmx root mode in this logical core
	GuestState[currentProcessorIndex].IsOnVmxRoot = true;
	Ept* currentEptInstance = GuestState[currentProcessorIndex].EptInstance;

	__vmx_vmread(VM_EXIT_REASON, &exitReason);
	__vmx_vmread(EXIT_QUALIFICATION, &exitQualification);
	exitReason &= 0xffff;
	EventHandler::ClearPendingInjection();

	switch (exitReason) {
		case EXIT_REASON_EXCEPTION_NMI: {
			VMEXIT_INTERRUPT_INFO interruptExit = { 0 };
			SIZE_T interruptExitInfo = 0;
			__vmx_vmread(VM_EXIT_INTR_INFO, &interruptExitInfo);
			interruptExit.Flags = static_cast<UINT32>(interruptExitInfo);

			if (EventHandler::InjectEventFromVmExitInterruption(interruptExit))
				action = VmExitAction::InjectEvent;
			else
				LogUnhandledVmExit(exitReason, exitQualification);
			break;
		}
		case EXIT_REASON_TRIPLE_FAULT: {
			DbgBreakPoint();
			break;
		}

		case EXIT_REASON_HLT: {
			// __halt(); // We don't want to halt.
			action = VmExitAction::AdvanceRip;
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
			action = VmExitAction::AdvanceRip;
			break;
		}

		case EXIT_REASON_CR_ACCESS: {
			if (RegistersHandler::HandleCRAccess(guestRegisters))
				action = VmExitAction::AdvanceRip;
			else {
				EventHandler::InjectGeneralProtection();
				action = VmExitAction::InjectEvent;
			}
			break;
		}
		case EXIT_REASON_MSR_READ:{
			if (RegistersHandler::HandleMSRRead(guestRegisters))
				action = VmExitAction::AdvanceRip;
			else {
				EventHandler::InjectGeneralProtection();
				action = VmExitAction::InjectEvent;
			}
			break;
		}
		case EXIT_REASON_MSR_WRITE: {
			if (RegistersHandler::HandleMSRWrite(guestRegisters))
				action = VmExitAction::AdvanceRip;
			else {
				EventHandler::InjectGeneralProtection();
				action = VmExitAction::InjectEvent;
			}
			break;
		}
		case EXIT_REASON_CPUID: {
			if (RegistersHandler::HandleCpuid(guestRegisters))
				action = VmExitAction::AdvanceRip;
			break;
		}
		case EXIT_REASON_MONITOR_TRAP_FLAG: {
			if (GuestState[currentProcessorIndex].HookedPage) {
				currentEptInstance->HandleMonitorTrapFlag(GuestState[currentProcessorIndex].HookedPage);
				GuestState[currentProcessorIndex].HookedPage = NULL;
			}
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
			guestRegisters->rax = static_cast<UINT64>(STATUS_SUCCESS);
			if (IsSelfVmcall(guestRegisters->r10, guestRegisters->r11, guestRegisters->r12))
				guestRegisters->rax = VmcallHandler(guestRegisters->rcx, guestRegisters->rdx, guestRegisters->r8, guestRegisters->r9);
			else if (!HypercallHandler(currentEptInstance, guestRegisters, guestFxState))
				guestRegisters->rax = static_cast<UINT64>(STATUS_INVALID_PARAMETER);
			action = VmExitAction::AdvanceRip;
			break;
		}
		case EXIT_REASON_XSETBV: {
			if (EmulateXsetbv(guestRegisters))
				action = VmExitAction::AdvanceRip;
			else {
				EventHandler::InjectGeneralProtection();
				action = VmExitAction::InjectEvent;
			}
			break;
		}
		case EXIT_REASON_INVD: {
			__wbinvd();
			action = VmExitAction::AdvanceRip;
			break;
		}
		case EXIT_REASON_UMONITOR:
		case EXIT_REASON_UMWAIT: {
			action = VmExitAction::AdvanceRip;
			break;
		}
		default: {
			LogUnhandledVmExit(exitReason, exitQualification);
			DbgBreakPoint();
			break;
		}
	}

	if (GuestState[currentProcessorIndex].VmxoffState.IsVmxoffExecuted)
		action = VmExitAction::ShutdownVmx;

	if (action == VmExitAction::AdvanceRip)
		VmxHelper::ResumeToNextInstruction();

	if (action != VmExitAction::ShutdownVmx)
		EventHandler::ReinjectEventFromIdtVectoring();

	// Set indicator of Vmx noon root mode to false
	GuestState[currentProcessorIndex].IsOnVmxRoot = false;
	return GuestState[currentProcessorIndex].VmxoffState.IsVmxoffExecuted;
}

/*
* Description:
* VmResumeFailure is called only when the assembly VMRESUME instruction fails.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmResumeFailure() {
	SIZE_T errorCode = 0;
	__vmx_vmread(VM_INSTRUCTION_ERROR, &errorCode);
	__vmx_off();
	NovaHypervisorLog(TRACE_FLAG_ERROR, "VM resume failed with error code: 0x%llx", errorCode);
}
