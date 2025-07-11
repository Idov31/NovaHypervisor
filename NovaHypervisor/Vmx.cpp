#include "pch.h"
#include "Vmx.h"

/*
* Description:
* VmxInitialize is responsible for initializing the VMX.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @status [bool] -- True if VMX was initialized, else false.
*/
bool VmxInitialize() {
	if (!VmxInitializer())
		return false;
	KeGenericCallDpc(InitializeGuest, 0x0);

	if (!NT_SUCCESS(AsmVmxVmcall(VMCALL_TEST, 0x22, 0x333, 0x4444)))
		return false;
	return true;
}

/*
* Description:
* InitializeGuest is responsible for initializing the guest by saving the state and launching the VM.
*
* Parameters:
* @dpc			   [_In_ KDPC*]		-- The DPC.
* @deferredContext [_In_opt_ PVOID] -- Additional context.
* @systemArgument1 [_In_opt_ PVOID] -- The system argument 1, used to signal when the DPC is done.
* @systemArgument2 [_In_opt_ PVOID] -- The system argument 2, used to synchronize.
* 
* Returns:
* There is no return value.
*/
void InitializeGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(deferredContext);

	AsmVmxSaveState();
	KeSignalCallDpcSynchronize(systemArgument2);
	KeSignalCallDpcDone(systemArgument1);
}

/*
* Description:
* TerminateVmx is responsible for freeing all the memory and devirtualize the processors.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void TerminateVmx() {
	KeGenericCallDpc(TerminateGuest, 0x0);
	ULONG processorCount = KeQueryActiveProcessorCount(0);

	for (ULONG i = 0; i < processorCount; i++) {
		if (GuestState[i].EptInstance) {
			delete GuestState[i].EptInstance;
			GuestState[i].EptInstance = nullptr;
		}
	}
	FreeVirtualMemory(GuestState);
}

/*
* Description:
* TerminateGuest is responsible for devirtualize the processor.
*
* Parameters:
* @dpc			   [_In_ KDPC*]		-- The DPC.
* @deferredContext [_In_opt_ PVOID] -- Additional context.
* @systemArgument1 [_In_opt_ PVOID] -- The system argument 1, used to signal when the DPC is done.
* @systemArgument2 [_In_opt_ PVOID] -- The system argument 2, used to synchronize.
*
* Returns:
* There is no return value.
*/
void TerminateGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(deferredContext);

	if (!VmxTerminate())
		NovaHypervisorLog(TRACE_FLAG_ERROR, "There was an error terminating vmx");

	KeSignalCallDpcSynchronize(systemArgument2);
	KeSignalCallDpcDone(systemArgument1);
}

/*
* Description:
* VmxInitializer is responsible for allocating the guests and creating the EPT and allocate the needed structures for
* each logical core.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @status [bool] -- True if the VMX was initialized, else false.
*/
_IRQL_requires_max_(APC_LEVEL)
bool VmxInitializer() {
	ULONG processorIndex = 0;
	bool initialized = true;

	if (!VmxHelper::IsVmxSupported()) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "VMX is not supported in this machine");
		return false;
	}
	ULONG processorCount = KeQueryActiveProcessorCount(0);
	GuestState = AllocateVirtualMemory<VmState*>(sizeof(VmState) * processorCount, false);

	if (!GuestState) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Insufficient memory for GuestState allocation");
		return false;
	}

	for (; processorIndex < processorCount; processorIndex++) {
		__try {
			GuestState[processorIndex].EptInstance = new Ept();
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to create EPT instance for processor 0x%lx: 0x%llx", processorIndex, GetExceptionCode());
			initialized = false;
			break;
		}
	}

	if (!initialized) {
		for (ULONG i = 0; i < processorIndex; i++) {
			if (GuestState[i].EptInstance) {
				delete GuestState[i].EptInstance;
				GuestState[i].EptInstance = nullptr;
			}
		}
		FreeVirtualMemory(GuestState);
		return false;
	}

	// Allocating vmxon, vmcs, vmm stack and msr bitmap for each logical core.
	KeGenericCallDpc(reinterpret_cast<PKDEFERRED_ROUTINE>(AllocateVmStructures), 0x0);
	return true;
}

/*
* Description:
* VirtualizeProcessor is responsible for loading the VMCS and launching the VM for a logical core.
*
* Parameters:
* @guestStack [_In_ PVOID] -- The guest stack.
*
* Returns:
* @status	  [bool]	   -- Returns only false if the VM didn't launch.
*/
bool VirtualizeProcessor(_In_ PVOID guestStack) {
	bool success = true;
	ULONG64 errorCode = 0;
	ULONG processorId = KeGetCurrentProcessorNumber();

	do {
		if (GuestState[processorId].VmxonRegion == 0) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to get guest state for processor %d", processorId);
			success = false;
			break;
		}

		if (!VmxHelper::ClearVmcsState(&GuestState[processorId])) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to clear VMCS state for processor %d", processorId);
			success = false;
			break;
		}

		if (!VmxHelper::LoadVmcs(&GuestState[processorId])) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to load VMCS for processor %d", processorId);
			success = false;
			break;
		}

		success = SetupVmcs(&GuestState[processorId], guestStack);
	} while (false);

	if (success) {
		NovaHypervisorLog(TRACE_FLAG_INFO, "Launching VM on processor %d", processorId);
		GuestState[processorId].IsLaunched = true;
		__vmx_vmlaunch();
	}

	__vmx_off();
	__vmx_vmread(VM_INSTRUCTION_ERROR, &errorCode);
	NovaHypervisorLog(TRACE_FLAG_ERROR, "VM launch failed with error code 0x%llx", errorCode);
	GuestState[processorId].IsLaunched = false;
	return false;
}

/*
* Description:
* VmxTerminate is responsible for freeing the allocated memory for each logical core.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @status [bool] -- True if the VMX was terminated, else false.
*/
bool VmxTerminate() {
	NTSTATUS status = STATUS_SUCCESS;
	ULONG currentProcessorIndex = KeGetCurrentProcessorNumber();

	NovaHypervisorLog(TRACE_FLAG_INFO, "Terminating VMX for logical core %d", currentProcessorIndex);
	status = AsmVmxVmcall(VMCALL_VMXOFF, NULL, NULL, NULL);

	if (!NT_SUCCESS(status))
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to execute vmcall to turn off vmx");

	if (GuestState[currentProcessorIndex].VmxonRegion != 0) {
		MmFreeContiguousMemory(reinterpret_cast<PVOID>(GuestState[currentProcessorIndex].VmxonRegion));
		GuestState[currentProcessorIndex].VmxonRegion = 0;
	}

	if (GuestState[currentProcessorIndex].VmcsRegion != 0) {
		MmFreeContiguousMemory(reinterpret_cast<PVOID>(GuestState[currentProcessorIndex].VmcsRegion));
		GuestState[currentProcessorIndex].VmcsRegion = 0;
	}
	FreeVirtualMemory(reinterpret_cast<PVOID>(GuestState[currentProcessorIndex].VmmStack));
	FreeVirtualMemory(reinterpret_cast<PVOID>(GuestState[currentProcessorIndex].MsrBitmap));
	return NT_SUCCESS(status);
}

/*
* Description:
* SetupVmcs is responsible for setting up vmcs and launching the VM.
*
* Parameters:
* @state	  [_In_ VmState*] -- The VM state.
* @guestStack [_In_ PVOID]	  -- The guest stack.
*
* Returns:
* @status	  [bool]						-- True if the VM was launched, else false.
*/
bool SetupVmcs(_Inout_ VmState* state, _In_ PVOID guestStack) {
	SEGMENT_SELECTOR segmentSelector = { 0 };
	IA32_VMX_BASIC_MSR vmxBasicMsr = { 0 };
	vmxBasicMsr.All = __readmsr(MSR_IA32_VMX_BASIC);

	__vmx_vmwrite(HOST_ES_SELECTOR, AsmGetEs() & 0xF8);
	__vmx_vmwrite(HOST_CS_SELECTOR, AsmGetCs() & 0xF8);
	__vmx_vmwrite(HOST_SS_SELECTOR, AsmGetSs() & 0xF8);
	__vmx_vmwrite(HOST_DS_SELECTOR, AsmGetDs() & 0xF8);
	__vmx_vmwrite(HOST_FS_SELECTOR, AsmGetFs() & 0xF8);
	__vmx_vmwrite(HOST_GS_SELECTOR, AsmGetGs() & 0xF8);
	__vmx_vmwrite(HOST_TR_SELECTOR, AsmGetTr() & 0xF8);

	// Setting the link pointer to the required value for 4KB VMCS. 
	__vmx_vmwrite(VMCS_LINK_POINTER, ~0ULL);

	__vmx_vmwrite(GUEST_IA32_DEBUGCTL, __readmsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);
	__vmx_vmwrite(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(MSR_IA32_DEBUGCTL) >> 32);

	/* Time-stamp counter offset */
	__vmx_vmwrite(TSC_OFFSET, 0);

	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MASK, 0);
	__vmx_vmwrite(PAGE_FAULT_ERROR_CODE_MATCH, 0);

	__vmx_vmwrite(VM_EXIT_MSR_STORE_COUNT, 0);
	__vmx_vmwrite(VM_EXIT_MSR_LOAD_COUNT, 0);

	__vmx_vmwrite(VM_ENTRY_MSR_LOAD_COUNT, 0);
	__vmx_vmwrite(VM_ENTRY_INTR_INFO_FIELD, 0);

	PVOID gdtBase = reinterpret_cast<PVOID>(AsmGetGdtBase());

	VmxHelper::FillGuestSelectorData(gdtBase, ES, AsmGetEs());
	VmxHelper::FillGuestSelectorData(gdtBase, CS, AsmGetCs());
	VmxHelper::FillGuestSelectorData(gdtBase, SS, AsmGetSs());
	VmxHelper::FillGuestSelectorData(gdtBase, DS, AsmGetDs());
	VmxHelper::FillGuestSelectorData(gdtBase, FS, AsmGetFs());
	VmxHelper::FillGuestSelectorData(gdtBase, GS, AsmGetGs());
	VmxHelper::FillGuestSelectorData(gdtBase, LDTR, AsmGetLdtr());
	VmxHelper::FillGuestSelectorData(gdtBase, TR, AsmGetTr());

	__vmx_vmwrite(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));

	
	ULONG cpuBasedVmExecControls = VmxHelper::AdjustControls(CPU_BASED_ACTIVATE_MSR_BITMAP | CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, 
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PROCBASED_CTLS : MSR_IA32_VMX_PROCBASED_CTLS);
	__vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, cpuBasedVmExecControls);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "CPU Based VM Exec Controls (Based on MSR_IA32_VMX_PROCBASED_CTLS) : 0x%x", cpuBasedVmExecControls);

	ULONG secondaryProcBasedVmExecControls = VmxHelper::AdjustControls(CPU_BASED_CTL2_RDTSCP | CPU_BASED_CTL2_ENABLE_EPT | 
		CPU_BASED_CTL2_ENABLE_INVPCID | CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS | CPU_BASED_CTL2_ENABLE_VPID,
		MSR_IA32_VMX_PROCBASED_CTLS2);

	__vmx_vmwrite(SECONDARY_VM_EXEC_CONTROL, secondaryProcBasedVmExecControls);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Secondary CPU Based VM Exec Controls (Based on MSR_IA32_VMX_PROCBASED_CTLS2) : 0x%x", secondaryProcBasedVmExecControls);

	__vmx_vmwrite(PIN_BASED_VM_EXEC_CONTROL, VmxHelper::AdjustControls(0,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PINBASED_CTLS : MSR_IA32_VMX_PINBASED_CTLS));

	__vmx_vmwrite(VM_EXIT_CONTROLS, VmxHelper::AdjustControls(VM_EXIT_IA32E_MODE,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_EXIT_CTLS : MSR_IA32_VMX_EXIT_CTLS));

	__vmx_vmwrite(VM_ENTRY_CONTROLS, VmxHelper::AdjustControls(VM_ENTRY_IA32E_MODE,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_ENTRY_CTLS : MSR_IA32_VMX_ENTRY_CTLS));

	__vmx_vmwrite(CR0_GUEST_HOST_MASK, 0);
	__vmx_vmwrite(CR4_GUEST_HOST_MASK, 0);

	__vmx_vmwrite(CR0_READ_SHADOW, 0);
	__vmx_vmwrite(CR4_READ_SHADOW, 0);

	__vmx_vmwrite(GUEST_CR0, __readcr0());
	__vmx_vmwrite(GUEST_CR3, __readcr3());
	__vmx_vmwrite(GUEST_CR4, __readcr4());

	__vmx_vmwrite(GUEST_DR7, 0x400);

	__vmx_vmwrite(HOST_CR0, __readcr0());
	__vmx_vmwrite(HOST_CR4, __readcr4());
	__vmx_vmwrite(HOST_CR3, VmxHelper::FindSystemDirectoryTableBase());

	__vmx_vmwrite(GUEST_GDTR_BASE, AsmGetGdtBase());
	__vmx_vmwrite(GUEST_IDTR_BASE, AsmGetIdtBase());
	__vmx_vmwrite(GUEST_GDTR_LIMIT, AsmGetGdtLimit());
	__vmx_vmwrite(GUEST_IDTR_LIMIT, AsmGetIdtLimit());

	__vmx_vmwrite(GUEST_RFLAGS, AsmGetRflags());

	__vmx_vmwrite(GUEST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(GUEST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(GUEST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));
	
	VmxHelper::GetSegmentDescriptor(&segmentSelector, AsmGetTr(), reinterpret_cast<PVOID>(AsmGetGdtBase()));
	__vmx_vmwrite(HOST_TR_BASE, segmentSelector.BASE);

	__vmx_vmwrite(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
	__vmx_vmwrite(HOST_GS_BASE, __readmsr(MSR_GS_BASE));

	__vmx_vmwrite(HOST_GDTR_BASE, AsmGetGdtBase());
	__vmx_vmwrite(HOST_IDTR_BASE, AsmGetIdtBase());

	__vmx_vmwrite(HOST_IA32_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	__vmx_vmwrite(HOST_IA32_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	__vmx_vmwrite(HOST_IA32_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

	__vmx_vmwrite(MSR_BITMAP, state->MsrBitmapPhysical);

	__vmx_vmwrite(EPT_POINTER, state->EptInstance->GetEptPointerFlags());
	__vmx_vmwrite(VIRTUAL_PROCESSOR_ID, VPID_TAG);

	__vmx_vmwrite(GUEST_RSP, reinterpret_cast<SIZE_T>(guestStack));
	__vmx_vmwrite(GUEST_RIP, reinterpret_cast<SIZE_T>(AsmVmxRestoreState));

	UINT64 hostRsp = (state->VmmStack + VMM_STACK_SIZE - 1);
	hostRsp &= ~0xF;
	__vmx_vmwrite(HOST_RSP, hostRsp);
	__vmx_vmwrite(HOST_RIP, reinterpret_cast<SIZE_T>(AsmVmexitHandler));
	return true;
}

/*
* Description:
* VmxVmxoff is responsible for turning off the VMX.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmxVmxoff() {
	ULONG currentProcessorIndex = KeGetCurrentProcessorNumber();

	// Making sure that we are running with the correct guest CR3 and not the host CR3.
	SIZE_T GuestCr3 = 0;
	__vmx_vmread(GUEST_CR3, &GuestCr3);
	__writecr3(GuestCr3);

	SIZE_T guestRip = 0;
	SIZE_T guestRsp = 0;
	__vmx_vmread(GUEST_RIP, &guestRip);
	__vmx_vmread(GUEST_RSP, &guestRsp);

	SIZE_T exitInstructionLength = 0;
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &exitInstructionLength);
	guestRip += exitInstructionLength;

	// Set the previous registers states
	GuestState[currentProcessorIndex].VmxoffState.GuestRip = guestRip;
	GuestState[currentProcessorIndex].VmxoffState.GuestRsp = guestRsp;
	GuestState[currentProcessorIndex].VmxoffState.IsVmxoffExecuted = true;

	VmxHelper::RestoreRegisters();

	// Clearing VMCS state.
	VmxHelper::ClearVmcsState(&GuestState[currentProcessorIndex]);
	__vmx_off();

	GuestState[currentProcessorIndex].IsLaunched = false;
	VmxHelper::DisableVmxOperation();
}

/*
* Description:
* AllocateVmStructures is responsible for enabling VMX and allocating vmxon, vmcs, vmm stack and msr bitmap for each logical core.
*
* Parameters:
* @dpc			   [_In_ KDPC*]		-- The DPC.
* @deferredContext [_In_opt_ PVOID] -- Additional context.
* @systemArgument1 [_In_opt_ PVOID] -- The system argument 1, used to signal when the DPC is done.
* @systemArgument2 [_In_opt_ PVOID] -- The system argument 2, used to synchronize.
*
* Returns:
* @status		   [bool]		    -- True if the VMX was enabled and the memory was allocated, else false.
*/
bool AllocateVmStructures(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(deferredContext);

	ULONG currentProcessorId = KeGetCurrentProcessorNumber();
	NovaHypervisorLog(TRACE_FLAG_INFO, "Allocating VMX regions for logical core %d", currentProcessorId);
	VmxHelper::EnableVmxOperation();
	NovaHypervisorLog(TRACE_FLAG_INFO, "VMX operation enabled for logical core %d", currentProcessorId);

	if (!AllocateRegion(VMXON_REGION, &GuestState[currentProcessorId])) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Error in allocating memory for Vmxon region for logical core %d", currentProcessorId);
		return false;
	}
	if (!AllocateRegion(VMCS_REGION, &GuestState[currentProcessorId])) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Error in allocating memory for Vmcs region for logical core %d", currentProcessorId);
		return false;
	}

	GuestState[currentProcessorId].VmmStack = AllocateVirtualMemory<UINT64>(VMM_STACK_SIZE, false);

	if (!GuestState[currentProcessorId].VmmStack) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate VMM stack for processor %d", currentProcessorId);
		return false;
	}
	GuestState[currentProcessorId].MsrBitmap = AllocateVirtualMemory<UINT64>(PAGE_SIZE, false);

	if (!GuestState[currentProcessorId].MsrBitmap) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate MSR bitmap for processor %d", currentProcessorId);
		return false;
	}
	GuestState[currentProcessorId].MsrBitmapPhysical = GetPhysicalAddress(GuestState[currentProcessorId].MsrBitmap);

	KeSignalCallDpcSynchronize(systemArgument2);
	KeSignalCallDpcDone(systemArgument1);
	return true;
}

/*
* Description:
* AllocateRegion is responsible for allocating a VMX region.
*
* Parameters:
* @state	  [_Inout_ VmState*] -- The VM state to allocate the region for.
* @regionType [RegionType]		 -- The region type to allocate.
*
* Returns:
* @allocated  [bool]			 -- Returns true if allocated, else false.
*/
bool AllocateRegion(_In_ RegionType regionType, _Inout_ VmState* state) {
	int status = 0;
	bool success = true;
	PVOID region = NULL;
	UINT64 alignedRegionVirtual = 0;
	UINT64 alignedRegionPhysical = 0;
	UINT64 physicalAddress = 0;
	IA32_VMX_BASIC_MSR basic = { 0 };
	PHYSICAL_ADDRESS maxPhysicalAddress = { 0 };
	maxPhysicalAddress.QuadPart = MAXULONG64;
	KIRQL originalIrql = KeGetCurrentIrql();
	size_t regionSize = 0;

	switch (regionType) {
	case VMXON_REGION:
		regionSize = VMXON_SIZE * 2;
		break;
	case VMCS_REGION:
		regionSize = VMCS_SIZE * 2;
		break;
	default:
		return false;
	}

	if (originalIrql > DISPATCH_LEVEL)
		KeLowerIrql(DISPATCH_LEVEL);

	do {
		region = MmAllocateContiguousMemory(regionSize + PAGE_SIZE, maxPhysicalAddress);

		if (!region) {
			success = false;
			break;
		}
		alignedRegionVirtual = (UINT64)PAGE_ALIGN(region);

		RtlSecureZeroMemory(region, regionSize + PAGE_SIZE);
		physicalAddress = MmGetPhysicalAddress(region).QuadPart;
		alignedRegionPhysical = (UINT64)PAGE_ALIGN(physicalAddress);

		// Changing Revision Identifier
		basic.All = __readmsr(MSR_IA32_VMX_BASIC);

		if (alignedRegionVirtual)
			*(UINT64*)alignedRegionVirtual = basic.Fields.RevisionIdentifier;

		if (basic.Fields.MemoryType != 6) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Memory type is not write-back");
			success = false;
			break;
		}

		if (regionType == VMXON_REGION)
			status = __vmx_on(&alignedRegionPhysical);

		if (status) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to do vmxon with status %d", status);
			success = false;
			break;
		}
	} while (false);

	// Making sure to free the allocated memory if failed with the right IRQL.
	if (!success) {
		if (region != 0)
			MmFreeContiguousMemory(region);
		region = NULL;
	}

	// Restoring the original IRQL.
	if (originalIrql > DISPATCH_LEVEL)
		KeRaiseIrql(originalIrql, &originalIrql);

	if (!success)
		return false;

	if (regionType == VMCS_REGION) {
		state->VmcsRegion = (UINT64)region;
		state->VmcsRegionPhysical = alignedRegionPhysical;
		return true;
	}
	else {
		state->VmxonRegion = (UINT64)region;
		state->VmxonRegionPhysical = alignedRegionPhysical;
	}
	return true;
}

/*
* Description:
* GetCurrentGuestRsp is responsible for getting the RSP for the current guest.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @rsp [UINT64] -- The RSP for the current guest.
*/
UINT64 GetCurrentGuestRsp() {
	return GuestState[KeGetCurrentProcessorNumber()].VmxoffState.GuestRsp;
}

/*
* Description:
* GetCurrentGuestRip is responsible for getting the RIP for the current guest.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @rip [UINT64] -- The RIP for the current guest.
*/
UINT64 GetCurrentGuestRip() {
	return GuestState[KeGetCurrentProcessorNumber()].VmxoffState.GuestRip;
}

/*
* Description:
* UnhookAllPagesDpc is responsible for removing all the EPT hooks and invalidating the TLB.
*
* Parameters:
* @dpc			   [_In_ KDPC*]		-- The DPC.
* @deferredContext [_In_opt_ PVOID] -- Additional context.
* @systemArgument1 [_In_opt_ PVOID] -- The system argument 1, used to signal when the DPC is done.
* @systemArgument2 [_In_opt_ PVOID] -- The system argument 2, used to synchronize.
*
* Returns:
* There is no return value.
*/
void UnhookAllPagesDpc(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);
	UNREFERENCED_PARAMETER(deferredContext);

	AsmVmxVmcall(VMCALL_UNHOOK_ALL_PAGES, NULL, NULL, NULL);
	KeSignalCallDpcSynchronize(systemArgument2);
	KeSignalCallDpcDone(systemArgument1);
}

/*
* Description:
* UnhookAllPages is responsible for removing single EPT hook and invalidating the TLB.
*
* Parameters:
* @dpc			   [_In_ KDPC*]		-- The DPC.
* @deferredContext [_In_opt_ PVOID] -- Additional context (page to unhook).
* @systemArgument1 [_In_opt_ PVOID] -- The system argument 1, used to signal when the DPC is done.
* @systemArgument2 [_In_opt_ PVOID] -- The system argument 2, used to synchronize.
*
* Returns:
* There is no return value.
*/
void UnhookSinglePage(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);

	if (deferredContext)
		AsmVmxVmcall(VMCALL_UNHOOK_SINGLE_PAGE, reinterpret_cast<UINT64>(deferredContext), NULL, NULL);
	KeSignalCallDpcSynchronize(systemArgument2);
	KeSignalCallDpcDone(systemArgument1);
}