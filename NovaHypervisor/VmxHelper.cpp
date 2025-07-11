#include "pch.h"
#include "VmxHelper.h"
#include "GlobalVariables.h"

/*
* Description:
* EnableVmxOperation is responsible for enabling VMX operation.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmxHelper::EnableVmxOperation() {
	ULONGLONG cr4 = __readcr4();
	cr4 |= 0x2000;
	__writecr4(cr4);
};

/*
* Description:
* DisableVmxOperation is responsible for disabling VMX operation.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmxHelper::DisableVmxOperation() {
	ULONGLONG cr4 = __readcr4();
	cr4 &= ~0x2000;
	__writecr4(cr4);
};

/*
* Description:
* IsVmxSupported is responsible for getting whether VMX is supported.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @supported [bool] -- True if VMX is supported, else false.
*/
 bool VmxHelper::IsVmxSupported() {
	IA32_FEATURE_CONTROL_MSR control = { 0 };
	CPUID cpuidResult = { 0 };

	__cpuid((int*)&cpuidResult, 1);

	if ((cpuidResult.ecx & (1 << 5)) == 0)
		return false;
	control.All = __readmsr(MSR_IA32_FEATURE_CONTROL);

	if (control.Fields.Lock == 0) {
		control.Fields.Lock = TRUE;
		control.Fields.EnableVmxon = TRUE;
		__writemsr(MSR_IA32_FEATURE_CONTROL, control.All);
		return true;
	}
	return control.Fields.EnableVmxon;
}

/*
* Description:
* ClearVmcsState is responsible for setting VMCS launch state to clear.
*
* Parameters:
* @state  [_Inout_ VmState*] -- The VM state to clear.
*
* Returns:
* @status [bool]			 -- True if the VMCS was cleared, else false.
*/
 bool VmxHelper::ClearVmcsState(_Inout_ VmState* state) {
	int status = __vmx_vmclear(&state->VmcsRegionPhysical);

	if (status) {
		__vmx_off();
		return false;
	}
	return true;
}

/*
* Description:
* LoadVmcs is responsible for loading the VMCS.
*
* Parameters:
* @state  [_Inout_ VmState*] -- The VM state that contains the VMCS to load.
*
* Returns:
* @status [bool]			 -- True if the VMCS loaded, else false.
*/
 bool VmxHelper::LoadVmcs(_Inout_ VmState* state) {
	return !__vmx_vmptrld(&state->VmcsRegionPhysical);
}

/*
* Description:
* ResumeToNextInstruction is responsible for resuming to the next instruction after VMEXIT.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmxHelper::ResumeToNextInstruction() {
	SIZE_T resumeRip = NULL;
	SIZE_T currentRip = NULL;
	SIZE_T exitInstructionLength = 0;

	__vmx_vmread(GUEST_RIP, &currentRip);
	__vmx_vmread(VM_EXIT_INSTRUCTION_LEN, &exitInstructionLength);

	resumeRip = currentRip + exitInstructionLength;
	__vmx_vmwrite(GUEST_RIP, resumeRip);
}

/*
* Description:
* GetSegmentDescriptor is responsible for filling the segment selector with the base, limit, and attributes of the segment.
*
* Parameters:
* @segmentSelector [_Inout_ PSEGMENT_SELECTOR] -- The segment selector to fill.
* @selector		   [_In_ USHORT]			   -- The selector to get the descriptor for.
* @gdtBase		   [_In_ PVOID]				   -- The base of the GDT.
*
* Returns:
* @status		   [bool]					   -- True if the segment descriptor was filled, else false.
*/
 bool VmxHelper::GetSegmentDescriptor(_Inout_ PSEGMENT_SELECTOR segmentSelector, _In_ USHORT selector, _In_ PVOID gdtBase) {
	if (!segmentSelector || selector & 4)
		return false;
	PSEGMENT_DESCRIPTOR segDesc = reinterpret_cast<PSEGMENT_DESCRIPTOR>(reinterpret_cast<PUCHAR>(gdtBase) + (selector & ~0x7));

	segmentSelector->SEL = selector;
	segmentSelector->BASE = segDesc->BASE0 | segDesc->BASE1 << 16 | segDesc->BASE2 << 24;
	segmentSelector->LIMIT = segDesc->LIMIT0 | (segDesc->LIMIT1ATTR1 & 0xf) << 16;
	segmentSelector->ATTRIBUTES.UCHARs = segDesc->ATTR0 | (segDesc->LIMIT1ATTR1 & 0xf0) << 4;

	// TSS or callgate, save the base high part.
	if (!(segDesc->ATTR0 & 0x10)) {
		ULONG64 tmp = *reinterpret_cast<PULONG64>((reinterpret_cast<PUCHAR>(segDesc) + 8));
		segmentSelector->BASE = (segmentSelector->BASE & 0xffffffff) | (tmp << 32);
	}

	// 4096-bit granularity is enabled for this segment, scale the limit
	if (segmentSelector->ATTRIBUTES.Fields.G)
		segmentSelector->LIMIT = (segmentSelector->LIMIT << 12) + 0xfff;
	return true;
}

/*
* Description:
* FillGuestSelectorData is responsible for setting the guest selector, attributes, limit, and base
*
* Parameters:
* @gdtBase		   [_In_ PVOID]	 -- The base of the GDT.
* @segmentRegister [_In_ ULONG]	 -- The segment register to set.
* @selector		   [_In_ USHORT] -- The selector to get the descriptor for.
*
* Returns:
* There is no return value.
*/
 bool VmxHelper::FillGuestSelectorData(_In_ PVOID gdtBase, _In_ ULONG segmentRegister, _In_ USHORT selector) {
	SEGMENT_SELECTOR segmentSelector = { 0 };
	ULONG accessRights = 0;

	if (!GetSegmentDescriptor(&segmentSelector, selector, gdtBase))
		return false;
	accessRights = reinterpret_cast<PUCHAR>(&segmentSelector.ATTRIBUTES)[0];
	accessRights += (reinterpret_cast<PUCHAR>(&segmentSelector.ATTRIBUTES)[1] << 12);

	if (selector == 0)
		accessRights |= 0x10000;
	__vmx_vmwrite(GUEST_ES_SELECTOR + segmentRegister * 2, selector);
	__vmx_vmwrite(GUEST_ES_LIMIT + segmentRegister * 2, segmentSelector.LIMIT);
	__vmx_vmwrite(GUEST_ES_AR_BYTES + segmentRegister * 2, accessRights);
	__vmx_vmwrite(GUEST_ES_BASE + segmentRegister * 2, segmentSelector.BASE);
	return true;
}

/*
* Description:
* AdjustControls is responsible for adjusting MSR controls.
*
* Parameters:
* @ctl	[_In_ ULONG] -- Control to adjust.
* @msr	[_In_ ULONG] -- The MSR to adjust.
*
* Returns:
* @ctl	[ULONG]		 -- The adjusted control.
*/
 ULONG VmxHelper::AdjustControls(_In_ ULONG ctl, _In_ ULONG msr) {
	MSR msrValue = { 0 };
	msrValue.Content = __readmsr(msr);
	ctl &= msrValue.High;
	ctl |= msrValue.Low;
	return ctl;
}

/*
* Description:
* InvalidateVpid is responsible for invalidating single vpid, single address or all VPIDs.
*
* Parameters:
* @vpid	   [_In_opt_ UINT64] -- The vpid to invalidate.
* @address [_In_opt_ UINT64] -- The address to invalidate.
*
* Returns:
* There is no return value.
*/
void VmxHelper::InvalidateVpid(_In_opt_ UINT64 vpid, _In_opt_ UINT64 address) {
	INVVPID_DESCRIPTOR descriptor = { 0 };
	InvvpidType type = InvvpidAllContext;

	if (vpid) {
		descriptor.Vpid = vpid;
		type = InvvpidSingleContext;

		if (address) {
			descriptor.LinearAddress = address;
			type = InvvpidIndividualAddress;
		}
	}
	AsmInvvpid(type, &descriptor);
}

/*
* Description:
* InvalidateEpt is responsible for invalidating single or all EPT contexts.
*
* Parameters:
* @context [_In_opt_ UINT64] -- The context to invalidate.
*
* Returns:
* There is no return value.
*/
void VmxHelper::InvalidateEpt(_In_opt_ UINT64 context) {
	INVEPT_DESC descriptor = { 0 };
	ULONG inveptType = ALL_CONTEXTS;

	if (context) {
		descriptor.EptPointer.Flags = context;
		inveptType = SINGLE_CONTEXT;
	}
	AsmInvept(inveptType, &descriptor);
}

/*
* Description:
* InvalidateEptByVmcall is responsible for invalidating single or all EPT contexts via vmcall.
*
* Parameters:
* @context [_In_opt_ UINT64] -- The context to invalidate.
*
* Returns:
* @status  [NTSTATUS]		 -- STATUS_SUCCESS if the operation was successful, else an error code.
*/
_Use_decl_annotations_
NTSTATUS VmxHelper::InvalidateEptByVmcall(_In_opt_ UINT64 context) {
	return context ? AsmVmxVmcall(VMCALL_INVEPT_SINGLE_CONTEXT, context, NULL, NULL) :
		AsmVmxVmcall(VMCALL_INVEPT_ALL_CONTEXT, NULL, NULL, NULL);
}

/*
* Description:
* HookPageByVmcall is responsible for hooking a page in VMX root mode via vmcall.
*
* Parameters:
* @context [_In_opt_ UINT64] -- The hooked page details.
*
* Returns:
* @status  [NTSTATUS]		 -- STATUS_SUCCESS if the operation was successful, else an error code.
*/
_Use_decl_annotations_
NTSTATUS VmxHelper::HookPageByVmcall(_In_opt_ UINT64 context) {
	if (!context)
		return STATUS_INVALID_PARAMETER;
	HookedPage* hookedPage = reinterpret_cast<HookedPage*>(context);
	return AsmVmxVmcall(VMCALL_EXEC_HOOK_PAGE, hookedPage->Address, hookedPage->Permissions, NULL);
}

/*
* Description:
* UnhookPageByVmcall is responsible for unhooking a page in VMX root mode via vmcall.
*
* Parameters:
* @context [_In_opt_ UINT64] -- The address to unhook.
*
* Returns:
* @status  [NTSTATUS]		 -- STATUS_SUCCESS if the operation was successful, else an error code.
*/
_Use_decl_annotations_
NTSTATUS VmxHelper::UnhookPageByVmcall(_In_opt_ UINT64 context) {
	if (!context)
		return STATUS_INVALID_PARAMETER;

	return AsmVmxVmcall(VMCALL_UNHOOK_SINGLE_PAGE, context, NULL, NULL);
}

/*
* Description:
* RestoreRegisters is responsible for restoring the fs, gs, gdtr, gdtr limit, idtr and idtr limit when vmxoff is performed.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void VmxHelper::RestoreRegisters() {
	ULONG64 fsBase = 0;
	ULONG64 gsBase = 0;
	ULONG64 gdtrBase = 0;
	ULONG64 gdtrLimit = 0;
	ULONG64 idtrBase = 0;
	ULONG64 idtrLimit = 0;

	__vmx_vmread(GUEST_FS_BASE, &fsBase);
	__writemsr(MSR_FS_BASE, fsBase);

	__vmx_vmread(GUEST_GS_BASE, &gsBase);
	__writemsr(MSR_GS_BASE, gsBase);

	__vmx_vmread(GUEST_GDTR_BASE, &gdtrBase);
	__vmx_vmread(GUEST_GDTR_LIMIT, &gdtrLimit);

	AsmReloadGdtr(reinterpret_cast<PVOID>(gdtrBase), gdtrLimit);

	__vmx_vmread(GUEST_IDTR_BASE, &idtrBase);
	__vmx_vmread(GUEST_IDTR_LIMIT, &idtrLimit);

	AsmReloadIdtr(reinterpret_cast<PVOID>(idtrBase), idtrLimit);
}

/*
* Description:
* FindSystemDirectoryTableBase is responsible for getting the system process' directory table base.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @directoryTableBase [UINT64] -- The system process' directory table base.
*/
 UINT64 VmxHelper::FindSystemDirectoryTableBase() {
	KPROCESS* SystemProcess = static_cast<PKPROCESS>(PsInitialSystemProcess);
	return SystemProcess->DirectoryTableBase;
}

 /*
 * Description:
 * FindKernelBaseAddress is responsible for finding the kernel base address.
 * 
 * Parameters:
 * There are no parameters.
 * 
 * Returns:
 * @status [NTSTATUS] -- STATUS_SUCCESS if the kernel base address was found, else error.
 */
 NTSTATUS VmxHelper::FindKernelBaseAddress() {
	 PKLDR_DATA_TABLE_ENTRY loadedModulesEntry = NULL;
	 NTSTATUS status = STATUS_NOT_FOUND;

	 if (!ExAcquireResourceExclusiveLite(PsLoadedModuleResource, 1))
		 return STATUS_ABANDONED;

	 for (PLIST_ENTRY pListEntry = PsLoadedModuleList->InLoadOrderLinks.Flink;
		 pListEntry != &PsLoadedModuleList->InLoadOrderLinks;
		 pListEntry = pListEntry->Flink) {
		 loadedModulesEntry = CONTAINING_RECORD(pListEntry, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

		 if (_wcsnicmp(loadedModulesEntry->BaseDllName.Buffer, KERNEL_NAME, KERNEL_NAME_LEN) == 0) {
			 KernelBaseInfo.KernelBaseAddress = reinterpret_cast<UINT64>(loadedModulesEntry->DllBase);
			 KernelBaseInfo.KernelSize = loadedModulesEntry->SizeOfImageNotRounded;
			 status = STATUS_SUCCESS;
			 break;
		 }
	 }

	 ExReleaseResourceLite(PsLoadedModuleResource);
	 return status;
 }

 void VmxHelper::SetMonitorTrapFlag(_In_ bool set) {
	 ULONG64 cpuBasedVmExecControls = 0;
	 __vmx_vmread(CPU_BASED_VM_EXEC_CONTROL, &cpuBasedVmExecControls);
	 cpuBasedVmExecControls = set ? cpuBasedVmExecControls | CPU_BASED_MONITOR_TRAP_FLAG :
		 cpuBasedVmExecControls & ~CPU_BASED_MONITOR_TRAP_FLAG;
	 __vmx_vmwrite(CPU_BASED_VM_EXEC_CONTROL, cpuBasedVmExecControls);
 }