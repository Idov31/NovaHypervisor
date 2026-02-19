#include "pch.h"
#include "RegistersHandler.h"

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
	return (rcx <= 0x00001FFF || (rcx >= 0xC0000000 && rcx <= 0xC0001FFF)) ||
		(rcx >= RESERVED_MSR_RANGE_LOW && rcx <= RESERVED_MSR_RANGE_HIGH);
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
void RegistersHandler::HandleCRAccess(_In_ PGUEST_REGS guestRegisters) {
	SIZE_T exitQualification = 0;
	__vmx_vmread(EXIT_QUALIFICATION, &exitQualification);

	PMOV_CR_QUALIFICATION data = (PMOV_CR_QUALIFICATION)&exitQualification;
	PULONG64 registerPointer = &guestRegisters->rax + data->Fields.Register;

	// Point it to guest RSP
	if (data->Fields.Register == 4) {
		SIZE_T rsp = 0;
		__vmx_vmread(GUEST_RSP, &rsp);
		*registerPointer = rsp;
	}

	if (data->Fields.AccessType > TYPE_MOV_FROM_CR) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported CR access type: %d", data->Fields.AccessType);
		return;
	}

	if (data->Fields.AccessType == TYPE_MOV_TO_CR) {
		switch (data->Fields.ControlRegister) {
		case 0: {
			__vmx_vmwrite(GUEST_CR0, *registerPointer);
			__vmx_vmwrite(CR0_READ_SHADOW, *registerPointer);
			break;
		}
		case 3: {
			__vmx_vmwrite(GUEST_CR3, (*registerPointer & ~(1ULL << 63)));
			VmxHelper::InvalidateVpid(1);
			break;
		}
		case 4: {
			__vmx_vmwrite(GUEST_CR4, *registerPointer);
			__vmx_vmwrite(CR4_READ_SHADOW, *registerPointer);
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported write operation for control register: %d", data->Fields.ControlRegister);
			break;
		}
		}
	}
	else {
		switch (data->Fields.ControlRegister) {
		case 0: {
			__vmx_vmread(GUEST_CR0, registerPointer);
			break;
		}
		case 3: {
			__vmx_vmread(GUEST_CR3, registerPointer);
			break;
		}
		case 4: {
			__vmx_vmread(GUEST_CR4, registerPointer);
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Unsupported read operation for control register: %d", data->Fields.ControlRegister);
			break;
		}
		}
	}
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
void RegistersHandler::HandleMSRRead(_Inout_ PGUEST_REGS guestRegisters) {
	MSR msr = { 0 };

	// Hyper-V synthetic MSRs - pass through transparently to the real hypervisor (TLFS 2.4)
	if (IsHyperVSyntheticMsr(guestRegisters->rcx)) {
		msr.Content = __readmsr(guestRegisters->rcx);
		guestRegisters->rax = static_cast<ULONG64>(msr.Low);
		guestRegisters->rdx = static_cast<ULONG64>(msr.High);
		return;
	}

	if (IsValidMsr(guestRegisters->rcx))
		msr.Content = __readmsr((ULONG)guestRegisters->rcx);

	guestRegisters->rax = (ULONG64)msr.Low;
	guestRegisters->rdx = (ULONG64)msr.High;
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
void RegistersHandler::HandleMSRWrite(_In_ PGUEST_REGS guestRegisters) {
	// Hyper-V synthetic MSRs - pass through transparently to the real hypervisor (TLFS 2.4)
	if (IsHyperVSyntheticMsr(guestRegisters->rcx)) {
		MSR msr = { 0 };
		msr.Low = static_cast<ULONG>(guestRegisters->rax);
		msr.High = static_cast<ULONG>(guestRegisters->rdx);
		__writemsr(static_cast<ULONG>(guestRegisters->rcx), msr.Content);
		return;
	}

	if (IsValidMsr(guestRegisters->rcx)) {
		MSR msr = { 0 };
		msr.Low = static_cast<ULONG>(guestRegisters->rax);
		msr.High = static_cast<ULONG>(guestRegisters->rdx);
		__writemsr(static_cast<ULONG>(guestRegisters->rcx), msr.Content);
	}
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
void RegistersHandler::HandleCpuid(_Inout_ PGUEST_REGS guestRegisters) {
	int cpuInfo[4] = { 0 };

	__cpuidex(cpuInfo, static_cast<int>(guestRegisters->rax), static_cast<int>(guestRegisters->rcx));

	if (guestRegisters->rax == CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS) {
		cpuInfo[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;
	}
	else if (guestRegisters->rax == HYPERV_CPUID_INTERFACE) {
		cpuInfo[0] = '0#vH';
	}

	guestRegisters->rax = cpuInfo[0];
	guestRegisters->rbx = cpuInfo[1];
	guestRegisters->rcx = cpuInfo[2];
	guestRegisters->rdx = cpuInfo[3];
}