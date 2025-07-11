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
	if (IsValidMsr(guestRegisters->rcx)) {
		MSR msr = { 0 };
		msr.Low = (ULONG)guestRegisters->rax;
		msr.High = (ULONG)guestRegisters->rdx;
		__writemsr((ULONG)guestRegisters->rcx, msr.Content);
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

	__cpuidex(cpuInfo, (int)guestRegisters->rax, (int)guestRegisters->rcx);

	// If rax is 1, then need to set the hypervisor present bit. 
	// If it is cpuid_interface then need to return the interface identifier.
	if (guestRegisters->rax == CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS)
		cpuInfo[2] |= HYPERV_HYPERVISOR_PRESENT_BIT;
	else if (guestRegisters->rax == HYPERV_CPUID_INTERFACE)
		cpuInfo[0] = '0#vH'; // For hyperv support.
	else if (guestRegisters->rax == HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS) {
		cpuInfo[0] = HYPERV_CPUID_INTERFACE;
		cpuInfo[1] = HYPERVISOR_INTERFACE;
	}

	guestRegisters->rax = cpuInfo[0];
	guestRegisters->rbx = cpuInfo[1];
	guestRegisters->rcx = cpuInfo[2];
	guestRegisters->rdx = cpuInfo[3];
}