#include "pch.h"
#include "RegistersHandler.h"

bool RegistersHandler::IsHyperVSyntheticMsr(_In_ ULONG64 msr) {
	switch (msr) {
	case HV_X64_MSR_GUEST_OS_ID:
	case HV_X64_MSR_HYPERCALL:
	case HV_X64_MSR_VP_INDEX:
	case HV_X64_MSR_RESET:
	case HV_X64_MSR_VP_RUNTIME:
	case HV_X64_MSR_TIME_REF_COUNT:
	case HV_X64_MSR_REFERENCE_TSC:
	case HV_X64_MSR_TSC_FREQUENCY:
	case HV_X64_MSR_APIC_FREQUENCY:
	case HV_X64_MSR_NPIEP_CONFIG:
	case HV_X64_MSR_GUEST_IDLE:
	case HV_X64_MSR_REENLIGHTENMENT_CONTROL:
	case HV_X64_MSR_TSC_EMULATION_CONTROL:
	case HV_X64_MSR_TSC_EMULATION_STATUS:
	case HV_X64_MSR_STIME_UNHALTED_TIMER_CONFIG:
	case HV_X64_MSR_STIME_UNHALTED_TIMER_COUNT:
	case HV_X64_MSR_NESTED_VP_INDEX:
		return true;
	default:
		return (msr >= HV_X64_MSR_EOI && msr <= HV_X64_MSR_TPR) ||
			(msr >= HV_X64_MSR_SCONTROL && msr <= HV_X64_MSR_EOM) ||
			(msr >= HV_X64_MSR_SINT0 && msr <= HV_X64_MSR_SINT15) ||
			(msr >= HV_X64_MSR_STIMER0_CONFIG && msr <= HV_X64_MSR_STIMER3_COUNT) ||
			(msr >= HV_X64_MSR_CRASH_P0 && msr <= HV_X64_MSR_CRASH_CTL) ||
			(msr >= HV_X64_MSR_NESTED_SCONTROL && msr <= HV_X64_MSR_NESTED_EOM) ||
			(msr >= HV_X64_MSR_NESTED_SINT0 && msr <= HV_X64_MSR_NESTED_SINT15);
	}
}

/*
* Description:
* IsValidMsr is responsible for checking if an MSR is valid.
*
* Parameters:
* @rcx [_In_ ULONG64] -- RCX.
*
* Returns:
* Returns true if the MSR is valid, otherwise false.
*/
bool RegistersHandler::IsValidMsr(_In_ ULONG64 rcx) {
	return rcx <= 0x00001FFF || (rcx >= 0xC0000000 && rcx <= 0xC0001FFF) ||
		IsHyperVSyntheticMsr(rcx);
}

/*
* Description:
* HandleCRAccess is responsible for handling a control register access.
*
* Parameters:
* @guestRegisters [_In_ PGUEST_REGS] -- The guest registers.
*
* Returns:
* There is no return value
*/
bool RegistersHandler::HandleCRAccess(_In_ PGUEST_REGS guestRegisters) {
	SIZE_T exitQualification = 0;
	__vmx_vmread(EXIT_QUALIFICATION, &exitQualification);

	PMOV_CR_QUALIFICATION data = (PMOV_CR_QUALIFICATION)&exitQualification;
	PULONG64 registerPointer = nullptr;
	ULONG64 registerValue = 0;

	if (data->Fields.Register == 4) {
		SIZE_T rsp = 0;
		__vmx_vmread(GUEST_RSP, &rsp);
		registerValue = rsp;
	}
	else {
		registerPointer = &guestRegisters->rax + data->Fields.Register;
		registerValue = *registerPointer;
	}

	if (data->Fields.AccessType > TYPE_MOV_FROM_CR) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported CR access type: %d", data->Fields.AccessType);
		return false;
	}

	if (data->Fields.AccessType == TYPE_MOV_TO_CR) {
		switch (data->Fields.ControlRegister) {
		case 0: {
			__vmx_vmwrite(GUEST_CR0, registerValue);
			__vmx_vmwrite(CR0_READ_SHADOW, registerValue);
			break;
		}
		case 3: {
			__vmx_vmwrite(GUEST_CR3, (registerValue & ~(1ULL << 63)));
			VmxHelper::InvalidateVpid(VPID_TAG);
			break;
		}
		case 4: {
			__vmx_vmwrite(GUEST_CR4, registerValue);
			__vmx_vmwrite(CR4_READ_SHADOW, registerValue);
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported write operation for control register: %d", data->Fields.ControlRegister);
			return false;
		}
		}
	}
	else {
		switch (data->Fields.ControlRegister) {
		case 0: {
			__vmx_vmread(GUEST_CR0, &registerValue);
			break;
		}
		case 3: {
			__vmx_vmread(GUEST_CR3, &registerValue);
			break;
		}
		case 4: {
			__vmx_vmread(GUEST_CR4, &registerValue);
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported read operation for control register: %d", data->Fields.ControlRegister);
			return false;
		}
		}
		if (data->Fields.Register == 4)
			__vmx_vmwrite(GUEST_RSP, registerValue);
		else
			*registerPointer = registerValue;
	}
	return true;
}

/*
* Description:
* HandleMSRRead is responsible for handling MSR reading.
*
* Parameters:
* @guestRegisters [_Inout_ PGUEST_REGS] -- The guest registers.
*
* Returns:
* There is no return value
*/
bool RegistersHandler::HandleMSRRead(_Inout_ PGUEST_REGS guestRegisters) {
	MSR msr = { 0 };

	// Hyper-V synthetic MSRs - pass through transparently to the real hypervisor (TLFS 2.4)
	if (IsHyperVSyntheticMsr(guestRegisters->rcx)) {
		msr.Content = __readmsr(static_cast<ULONG>(guestRegisters->rcx));
		guestRegisters->rax = static_cast<ULONG64>(msr.Low);
		guestRegisters->rdx = static_cast<ULONG64>(msr.High);
		return true;
	}

	if (!IsValidMsr(guestRegisters->rcx)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Guest attempted to read unsupported MSR: 0x%llx", guestRegisters->rcx);
		return false;
	}

	msr.Content = __readmsr((ULONG)guestRegisters->rcx);

	guestRegisters->rax = (ULONG64)msr.Low;
	guestRegisters->rdx = (ULONG64)msr.High;
	return true;
}

/*
* Description:
* HandleMSRWrite is responsible for handling MSR writing.
*
* Parameters:
* @guestRegisters [_In_ PGUEST_REGS] -- The guest registers.
*
* Returns:
* There is no return value
*/
bool RegistersHandler::HandleMSRWrite(_In_ PGUEST_REGS guestRegisters) {
	// Hyper-V synthetic MSRs - pass through transparently to the real hypervisor (TLFS 2.4)
	if (IsHyperVSyntheticMsr(guestRegisters->rcx)) {
		MSR msr = { 0 };
		msr.Low = static_cast<ULONG>(guestRegisters->rax);
		msr.High = static_cast<ULONG>(guestRegisters->rdx);
		__writemsr(static_cast<ULONG>(guestRegisters->rcx), msr.Content);
		return true;
	}

	if (!IsValidMsr(guestRegisters->rcx)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Guest attempted to write unsupported MSR: 0x%llx", guestRegisters->rcx);
		return false;
	}

	MSR msr = { 0 };
	msr.Low = static_cast<ULONG>(guestRegisters->rax);
	msr.High = static_cast<ULONG>(guestRegisters->rdx);
	__writemsr(static_cast<ULONG>(guestRegisters->rcx), msr.Content);
	return true;
}

/*
* Description:
* HandleCpuid is responsible for handling cpuid instruction.
*
* Parameters:
* @guestRegisters [_Inout_ PGUEST_REGS] -- The guest registers.
*
* Returns:
* There is no return value.
*/
bool RegistersHandler::HandleCpuid(_Inout_ PGUEST_REGS guestRegisters) {
	int cpuInfo[4] = { 0 };

	__cpuidex(cpuInfo, static_cast<int>(guestRegisters->rax), static_cast<int>(guestRegisters->rcx));

	if (guestRegisters->rax == CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS) {
		cpuInfo[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;
	}
	else if (guestRegisters->rax == HYPERV_CPUID_INTERFACE) {
		cpuInfo[0] = '0#vH';
	}
	else if (guestRegisters->rax == HYPERV_CPUID_FEATURES) {
		cpuInfo[3] &= ~(HYPERV_CPUID_FEATURES_EDX_XMM_FAST_HYPERCALL_INPUT |
			HYPERV_CPUID_FEATURES_EDX_XMM_FAST_HYPERCALL_OUTPUT);
	}

	guestRegisters->rax = cpuInfo[0];
	guestRegisters->rbx = cpuInfo[1];
	guestRegisters->rcx = cpuInfo[2];
	guestRegisters->rdx = cpuInfo[3];
	return true;
}
