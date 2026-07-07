#include "pch.h"
#include "Vmx.h"

typedef struct _PROCESSOR_DPC_STATUS_CONTEXT {
	NTSTATUS* ProcessorStatuses;
	ULONG ProcessorCount;
} PROCESSOR_DPC_STATUS_CONTEXT, * PPROCESSOR_DPC_STATUS_CONTEXT;

static void InitializeProcessorStatuses(_Out_writes_(processorCount) NTSTATUS* processorStatuses, _In_ ULONG processorCount) {
	for (ULONG i = 0; i < processorCount; i++)
		processorStatuses[i] = STATUS_UNSUCCESSFUL;
}

static void SetCurrentProcessorStatus(_In_opt_ PVOID context, _In_ NTSTATUS status) {
	PPROCESSOR_DPC_STATUS_CONTEXT statusContext = static_cast<PPROCESSOR_DPC_STATUS_CONTEXT>(context);

	if (!statusContext || !statusContext->ProcessorStatuses)
		return;
	ULONG currentProcessor = KeGetCurrentProcessorNumber();

	if (currentProcessor < statusContext->ProcessorCount)
		statusContext->ProcessorStatuses[currentProcessor] = status;
}

static bool AreProcessorStatusesSuccessful(_In_reads_(processorCount) NTSTATUS* processorStatuses, _In_ ULONG processorCount, _In_ const char* operationName) {
	bool status = true;

	for (ULONG i = 0; i < processorCount; i++) {
		if (!NT_SUCCESS(processorStatuses[i])) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "%s failed on processor %d: 0x%08X", operationName, i, processorStatuses[i]);
			status = false;
		}
	}
	return status;
}

static bool HasAnyProcessorVmxState(_In_ ULONG processorCount) {
	if (!GuestState)
		return false;

	for (ULONG i = 0; i < processorCount; i++) {
		if (GuestState[i].IsLaunched || GuestState[i].IsVmxOn || GuestState[i].VmxonRegion)
			return true;
	}
	return false;
}

static void FreeProcessorVmResources(_Inout_ VmState* state) {
	if (!state)
		return;

	if (state->VmxonRegion != 0) {
		MmFreeContiguousMemory(reinterpret_cast<PVOID>(state->VmxonRegion));
		state->VmxonRegion = 0;
		state->VmxonRegionPhysical = 0;
	}

	if (state->VmcsRegion != 0) {
		MmFreeContiguousMemory(reinterpret_cast<PVOID>(state->VmcsRegion));
		state->VmcsRegion = 0;
		state->VmcsRegionPhysical = 0;
	}

	if (state->VmmStack) {
		FreeVirtualMemory(reinterpret_cast<PVOID>(state->VmmStack));
		state->VmmStack = 0;
	}

	if (state->MsrBitmap) {
		FreeVirtualMemory(reinterpret_cast<PVOID>(state->MsrBitmap));
		state->MsrBitmap = 0;
		state->MsrBitmapPhysical = 0;
	}
}

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
	ULONG processorCount = KeQueryActiveProcessorCount(0);
	NTSTATUS* launchStatuses = AllocateVirtualMemory<NTSTATUS*>(sizeof(NTSTATUS) * processorCount, false);

	if (!launchStatuses) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate VMX launch status array");
		TerminateVmx();
		return false;
	}
	InitializeProcessorStatuses(launchStatuses, processorCount);
	PROCESSOR_DPC_STATUS_CONTEXT launchContext = { launchStatuses, processorCount };
	KeGenericCallDpc(InitializeGuest, &launchContext);
	bool launched = AreProcessorStatusesSuccessful(launchStatuses, processorCount, "VM launch");
	FreeVirtualMemory(launchStatuses);

	if (!launched) {
		TerminateVmx();
		return false;
	}

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

	AsmVmxSaveState();
	SetCurrentProcessorStatus(deferredContext,
		(GuestState && GuestState[KeGetCurrentProcessorNumber()].IsLaunched) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL);

	if (systemArgument2)
		KeSignalCallDpcSynchronize(systemArgument2);

	if (systemArgument1)
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
	ULONG processorCount = KeQueryActiveProcessorCount(0);
	bool terminated = true;

	if (HasAnyProcessorVmxState(processorCount)) {
		NTSTATUS* terminationStatuses = AllocateVirtualMemory<NTSTATUS*>(sizeof(NTSTATUS) * processorCount, false);

		if (terminationStatuses) {
			InitializeProcessorStatuses(terminationStatuses, processorCount);
			PROCESSOR_DPC_STATUS_CONTEXT terminationContext = { terminationStatuses, processorCount };
			KeGenericCallDpc(TerminateGuest, &terminationContext);
			terminated = AreProcessorStatusesSuccessful(terminationStatuses, processorCount, "VMX termination");
			FreeVirtualMemory(terminationStatuses);
		}
		else {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate VMX termination status array");
			KeGenericCallDpc(TerminateGuest, 0x0);
			terminated = !HasAnyProcessorVmxState(processorCount);
		}
	}

	if (GuestState && terminated) {
		for (ULONG i = 0; i < processorCount; i++) {
			if (GuestState[i].EptInstance) {
				delete GuestState[i].EptInstance;
				GuestState[i].EptInstance = nullptr;
			}
		}
		FreeVirtualMemory(GuestState);
		GuestState = nullptr;
	}
	else if (GuestState) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Leaving VMX shared state allocated because one or more processors failed teardown");
	}
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
	NTSTATUS status = VmxTerminate() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

	if (!NT_SUCCESS(status))
		NovaHypervisorLog(TRACE_FLAG_ERROR, "There was an error terminating vmx");
	SetCurrentProcessorStatus(deferredContext, status);

	if (systemArgument2)
		KeSignalCallDpcSynchronize(systemArgument2);

	if (systemArgument1)
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

	if (!VmxHelper::IsXstateSaveAreaSupported()) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "XSTATE preservation requirements are not supported by this build");
		return false;
	}

	VmxHelper::InitializeVpidSupport();

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
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to create EPT instance for processor 0x%lx: 0x%08X", processorIndex, GetExceptionCode());
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
		GuestState = nullptr;
		return false;
	}

	NTSTATUS* allocationStatuses = AllocateVirtualMemory<NTSTATUS*>(sizeof(NTSTATUS) * processorCount, false);

	if (!allocationStatuses) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate VMX processor status array");
		for (ULONG i = 0; i < processorCount; i++) {
			if (GuestState[i].EptInstance) {
				delete GuestState[i].EptInstance;
				GuestState[i].EptInstance = nullptr;
			}
		}
		FreeVirtualMemory(GuestState);
		GuestState = nullptr;
		return false;
	}
	InitializeProcessorStatuses(allocationStatuses, processorCount);
	PROCESSOR_DPC_STATUS_CONTEXT allocationContext = { allocationStatuses, processorCount };

	// Allocating vmxon, vmcs, vmm stack and msr bitmap for each logical core.
	KeGenericCallDpc(reinterpret_cast<PKDEFERRED_ROUTINE>(AllocateVmStructures), &allocationContext);
	initialized = AreProcessorStatusesSuccessful(allocationStatuses, processorCount, "VMX structure allocation");
	FreeVirtualMemory(allocationStatuses);
	if (!initialized) {
		TerminateVmx();
		return false;
	}
	return initialized;
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
	bool vmcsLoaded = false;
	bool vmxoffExecuted = false;
	bool launchAttempted = false;
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
			vmxoffExecuted = true;
			success = false;
			break;
		}

		if (!VmxHelper::LoadVmcs(&GuestState[processorId])) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to load VMCS for processor %d", processorId);
			success = false;
			break;
		}
		vmcsLoaded = true;

		success = SetupVmcs(&GuestState[processorId], guestStack);
	} while (false);

	if (success) {
		NovaHypervisorLog(TRACE_FLAG_INFO, "Launching VM on processor %d", processorId);
		GuestState[processorId].IsLaunched = true;
		launchAttempted = true;
		__vmx_vmlaunch();
	}

	if (vmcsLoaded || launchAttempted)
		__vmx_vmread(VM_INSTRUCTION_ERROR, &errorCode);

	if (!vmxoffExecuted)
		__vmx_off();
	NovaHypervisorLog(TRACE_FLAG_ERROR, "VM launch failed with error code 0x%llx", errorCode);
	GuestState[processorId].IsLaunched = false;
	GuestState[processorId].IsVmxOn = false;
	VmxHelper::DisableVmxOperation();
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

	if (!GuestState)
		return true;
	VmState* state = &GuestState[currentProcessorIndex];

	if (state->IsLaunched) {
		status = AsmVmxVmcall(VMCALL_VMXOFF, NULL, NULL, NULL);

		if (!NT_SUCCESS(status) || state->IsLaunched) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to execute vmcall to turn off vmx");
			return false;
		}
	}
	else if (state->IsVmxOn) {
		bool vmxoffExecuted = false;

		if (state->VmcsRegionPhysical && !VmxHelper::ClearVmcsState(state))
			vmxoffExecuted = true;

		if (!vmxoffExecuted)
			__vmx_off();
		state->IsVmxOn = false;
		state->IsLaunched = false;
		VmxHelper::DisableVmxOperation();
	}

	FreeProcessorVmResources(state);
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

	if (!state || !guestStack || !state->EptInstance || !state->MsrBitmapPhysical)
		return false;

#define VMX_WRITE_OR_FAIL(Field, Value) \
	do { \
		if (!VmxHelper::WriteVmcsField((Field), static_cast<SIZE_T>(Value))) \
			return false; \
	} while (false)

	VMX_WRITE_OR_FAIL(HOST_ES_SELECTOR, AsmGetEs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_CS_SELECTOR, AsmGetCs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_SS_SELECTOR, AsmGetSs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_DS_SELECTOR, AsmGetDs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_FS_SELECTOR, AsmGetFs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_GS_SELECTOR, AsmGetGs() & 0xF8);
	VMX_WRITE_OR_FAIL(HOST_TR_SELECTOR, AsmGetTr() & 0xF8);

	// Setting the link pointer to the required value for 4KB VMCS. 
	VMX_WRITE_OR_FAIL(VMCS_LINK_POINTER, ~0ULL);

	VMX_WRITE_OR_FAIL(GUEST_IA32_DEBUGCTL, __readmsr(MSR_IA32_DEBUGCTL) & 0xFFFFFFFF);
	VMX_WRITE_OR_FAIL(GUEST_IA32_DEBUGCTL_HIGH, __readmsr(MSR_IA32_DEBUGCTL) >> 32);

	/* Time-stamp counter offset */
	VMX_WRITE_OR_FAIL(TSC_OFFSET, 0);

	VMX_WRITE_OR_FAIL(PAGE_FAULT_ERROR_CODE_MASK, 0);
	VMX_WRITE_OR_FAIL(PAGE_FAULT_ERROR_CODE_MATCH, 0);
	VMX_WRITE_OR_FAIL(EXCEPTION_BITMAP, 0);
	VMX_WRITE_OR_FAIL(CR3_TARGET_COUNT, 0);

	VMX_WRITE_OR_FAIL(VM_EXIT_MSR_STORE_COUNT, 0);
	VMX_WRITE_OR_FAIL(VM_EXIT_MSR_LOAD_COUNT, 0);

	VMX_WRITE_OR_FAIL(VM_ENTRY_MSR_LOAD_COUNT, 0);
	VMX_WRITE_OR_FAIL(VM_ENTRY_INTR_INFO_FIELD, 0);

	PVOID gdtBase = reinterpret_cast<PVOID>(AsmGetGdtBase());

	if (!VmxHelper::FillGuestSelectorData(gdtBase, ES, AsmGetEs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, CS, AsmGetCs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, SS, AsmGetSs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, DS, AsmGetDs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, FS, AsmGetFs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, GS, AsmGetGs()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, LDTR, AsmGetLdtr()) ||
		!VmxHelper::FillGuestSelectorData(gdtBase, TR, AsmGetTr()))
		return false;

	VMX_WRITE_OR_FAIL(GUEST_FS_BASE, __readmsr(MSR_FS_BASE));
	VMX_WRITE_OR_FAIL(GUEST_GS_BASE, __readmsr(MSR_GS_BASE));

	
	ULONG cpuBasedVmExecControls = VmxHelper::AdjustControls(
		CPU_BASED_ACTIVATE_MSR_BITMAP | 
		CPU_BASED_ACTIVATE_SECONDARY_CONTROLS, 
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PROCBASED_CTLS : MSR_IA32_VMX_PROCBASED_CTLS);
	VMX_WRITE_OR_FAIL(CPU_BASED_VM_EXEC_CONTROL, cpuBasedVmExecControls);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "CPU Based VM Exec Controls (Based on MSR_IA32_VMX_PROCBASED_CTLS) : 0x%x", cpuBasedVmExecControls);

	ULONG requestedSecondaryControls = CPU_BASED_CTL2_RDTSCP |
		CPU_BASED_CTL2_ENABLE_EPT |
		CPU_BASED_CTL2_ENABLE_INVPCID |
		CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS |
		CPU_BASED_CTL2_ENABLE_USER_WAIT_AND_PAUSE;

	if (VpidSupported)
		requestedSecondaryControls |= CPU_BASED_CTL2_ENABLE_VPID;

	ULONG secondaryProcBasedVmExecControls = VmxHelper::AdjustControls(
		requestedSecondaryControls,
		MSR_IA32_VMX_PROCBASED_CTLS2);

	VMX_WRITE_OR_FAIL(SECONDARY_VM_EXEC_CONTROL, secondaryProcBasedVmExecControls);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Secondary CPU Based VM Exec Controls (Based on MSR_IA32_VMX_PROCBASED_CTLS2) : 0x%x", secondaryProcBasedVmExecControls);

	VMX_WRITE_OR_FAIL(PIN_BASED_VM_EXEC_CONTROL, VmxHelper::AdjustControls(0,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_PINBASED_CTLS : MSR_IA32_VMX_PINBASED_CTLS));

	const ULONG vmExitControls =
		VM_EXIT_IA32E_MODE |
		VM_EXIT_SAVE_GUEST_PAT |
		VM_EXIT_LOAD_HOST_PAT |
		VM_EXIT_SAVE_GUEST_EFER |
		VM_EXIT_LOAD_HOST_EFER;
	const ULONG vmEntryControls =
		VM_ENTRY_IA32E_MODE |
		VM_ENTRY_LOAD_GUEST_PAT |
		VM_ENTRY_LOAD_GUEST_EFER;

	VMX_WRITE_OR_FAIL(GUEST_IA32_PAT, __readmsr(MSR_IA32_PAT));
	VMX_WRITE_OR_FAIL(GUEST_IA32_EFER, __readmsr(MSR_IA32_EFER));
	VMX_WRITE_OR_FAIL(HOST_IA32_PAT, __readmsr(MSR_IA32_PAT));
	VMX_WRITE_OR_FAIL(HOST_IA32_EFER, __readmsr(MSR_IA32_EFER));

	VMX_WRITE_OR_FAIL(VM_EXIT_CONTROLS, VmxHelper::AdjustControls(vmExitControls,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_EXIT_CTLS : MSR_IA32_VMX_EXIT_CTLS));

	VMX_WRITE_OR_FAIL(VM_ENTRY_CONTROLS, VmxHelper::AdjustControls(vmEntryControls,
		vmxBasicMsr.Fields.VmxCapabilityHint ? MSR_IA32_VMX_TRUE_ENTRY_CTLS : MSR_IA32_VMX_ENTRY_CTLS));

	VMX_WRITE_OR_FAIL(CR0_GUEST_HOST_MASK, 0);
	VMX_WRITE_OR_FAIL(CR4_GUEST_HOST_MASK, 0);

	VMX_WRITE_OR_FAIL(CR0_READ_SHADOW, 0);
	VMX_WRITE_OR_FAIL(CR4_READ_SHADOW, 0);

	VMX_WRITE_OR_FAIL(GUEST_CR0, __readcr0());
	VMX_WRITE_OR_FAIL(GUEST_CR3, __readcr3());
	VMX_WRITE_OR_FAIL(GUEST_CR4, __readcr4());

	VMX_WRITE_OR_FAIL(GUEST_DR7, 0x400);

	VMX_WRITE_OR_FAIL(HOST_CR0, __readcr0());
	VMX_WRITE_OR_FAIL(HOST_CR4, __readcr4());
	VMX_WRITE_OR_FAIL(HOST_CR3, HostDirectoryTableBase);

	VMX_WRITE_OR_FAIL(GUEST_GDTR_BASE, AsmGetGdtBase());
	VMX_WRITE_OR_FAIL(GUEST_IDTR_BASE, AsmGetIdtBase());
	VMX_WRITE_OR_FAIL(GUEST_GDTR_LIMIT, AsmGetGdtLimit());
	VMX_WRITE_OR_FAIL(GUEST_IDTR_LIMIT, AsmGetIdtLimit());

	VMX_WRITE_OR_FAIL(GUEST_RFLAGS, AsmGetRflags());
	VMX_WRITE_OR_FAIL(GUEST_INTERRUPTIBILITY_INFO, 0);
	VMX_WRITE_OR_FAIL(GUEST_ACTIVITY_STATE, 0);
	VMX_WRITE_OR_FAIL(GUEST_PENDING_DBG_EXCEPTIONS, 0);
	VMX_WRITE_OR_FAIL(GUEST_SM_BASE, 0);

	VMX_WRITE_OR_FAIL(GUEST_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	VMX_WRITE_OR_FAIL(GUEST_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	VMX_WRITE_OR_FAIL(GUEST_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));
	
	if (!VmxHelper::GetSegmentDescriptor(&segmentSelector, AsmGetTr(), reinterpret_cast<PVOID>(AsmGetGdtBase())))
		return false;
	VMX_WRITE_OR_FAIL(HOST_TR_BASE, segmentSelector.BASE);

	VMX_WRITE_OR_FAIL(HOST_FS_BASE, __readmsr(MSR_FS_BASE));
	VMX_WRITE_OR_FAIL(HOST_GS_BASE, __readmsr(MSR_GS_BASE));

	VMX_WRITE_OR_FAIL(HOST_GDTR_BASE, AsmGetGdtBase());
	VMX_WRITE_OR_FAIL(HOST_IDTR_BASE, AsmGetIdtBase());

	VMX_WRITE_OR_FAIL(HOST_IA32_SYSENTER_CS, __readmsr(MSR_IA32_SYSENTER_CS));
	VMX_WRITE_OR_FAIL(HOST_IA32_SYSENTER_EIP, __readmsr(MSR_IA32_SYSENTER_EIP));
	VMX_WRITE_OR_FAIL(HOST_IA32_SYSENTER_ESP, __readmsr(MSR_IA32_SYSENTER_ESP));

	VMX_WRITE_OR_FAIL(MSR_BITMAP, state->MsrBitmapPhysical);

	VMX_WRITE_OR_FAIL(EPT_POINTER, state->EptInstance->GetEptPointerFlags());

	if (VpidSupported)
		VMX_WRITE_OR_FAIL(VIRTUAL_PROCESSOR_ID, VmxHelper::GetVpidTagForProcessor(KeGetCurrentProcessorNumber()));

	VMX_WRITE_OR_FAIL(GUEST_RSP, reinterpret_cast<SIZE_T>(guestStack));
	VMX_WRITE_OR_FAIL(GUEST_RIP, reinterpret_cast<SIZE_T>(AsmVmxRestoreState));

	UINT64 hostRsp = (state->VmmStack + VMM_STACK_SIZE - 1);
	hostRsp &= ~0xF;
	VMX_WRITE_OR_FAIL(HOST_RSP, hostRsp);
	VMX_WRITE_OR_FAIL(HOST_RIP, reinterpret_cast<SIZE_T>(AsmVmexitHandler));
	return true;
}

#undef VMX_WRITE_OR_FAIL

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
	bool vmxoffExecuted = false;
	if (!VmxHelper::ClearVmcsState(&GuestState[currentProcessorIndex]))
		vmxoffExecuted = true;

	if (!vmxoffExecuted)
		__vmx_off();

	GuestState[currentProcessorIndex].IsLaunched = false;
	GuestState[currentProcessorIndex].IsVmxOn = false;
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
* There is no return value.
*/
void AllocateVmStructures(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2) {
	UNREFERENCED_PARAMETER(dpc);

	ULONG currentProcessorId = KeGetCurrentProcessorNumber();
	NTSTATUS status = STATUS_SUCCESS;
	NovaHypervisorLog(TRACE_FLAG_INFO, "Allocating VMX regions for logical core %d", currentProcessorId);

	do {
		if (!GuestState) {
			status = STATUS_UNSUCCESSFUL;
			break;
		}
		VmState* state = &GuestState[currentProcessorId];

		VmxHelper::EnableVmxOperation();
		NovaHypervisorLog(TRACE_FLAG_INFO, "VMX operation enabled for logical core %d", currentProcessorId);

		if (!AllocateRegion(VMXON_REGION, state)) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Error in allocating memory for Vmxon region for logical core %d", currentProcessorId);
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		if (!AllocateRegion(VMCS_REGION, state)) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Error in allocating memory for Vmcs region for logical core %d", currentProcessorId);
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}

		state->VmmStack = AllocateVirtualMemory<UINT64>(VMM_STACK_SIZE, false);

		if (!state->VmmStack) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate VMM stack for processor %d", currentProcessorId);
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		state->MsrBitmap = AllocateVirtualMemory<UINT64>(PAGE_SIZE, false);

		if (!state->MsrBitmap) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate MSR bitmap for processor %d", currentProcessorId);
			status = STATUS_INSUFFICIENT_RESOURCES;
			break;
		}
		state->MsrBitmapPhysical = GetPhysicalAddress(state->MsrBitmap);

		if (!state->MsrBitmapPhysical)
			status = STATUS_UNSUCCESSFUL;
	} while (false);

	if (!NT_SUCCESS(status) && GuestState) {
		VmState* state = &GuestState[currentProcessorId];

		if (state->IsVmxOn) {
			bool vmxoffExecuted = false;

			if (state->VmcsRegionPhysical && !VmxHelper::ClearVmcsState(state))
				vmxoffExecuted = true;
			if (!vmxoffExecuted)
				__vmx_off();
			state->IsVmxOn = false;
			state->IsLaunched = false;
		}
		VmxHelper::DisableVmxOperation();
		FreeProcessorVmResources(state);
	}

	SetCurrentProcessorStatus(deferredContext, status);
	if (systemArgument2)
		KeSignalCallDpcSynchronize(systemArgument2);
	if (systemArgument1)
		KeSignalCallDpcDone(systemArgument1);
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
		state->IsVmxOn = true;
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

	if (systemArgument2)
		KeSignalCallDpcSynchronize(systemArgument2);

	if (systemArgument1)
		KeSignalCallDpcDone(systemArgument1);
}
