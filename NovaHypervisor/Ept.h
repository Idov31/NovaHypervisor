#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "InlineAsm.h"
#include "GlobalVariables.h"
#include "MemoryHelper.hpp"
#include "Vmx.h"
#include "ComLogger.h"

enum EptPagePermissions {
	EPT_PAGE_READ = 1,
	EPT_PAGE_WRITE = 2,
	EPT_PAGE_EXECUTE = 4,
	EPT_ALL_PERMISSIONS = EPT_PAGE_READ | EPT_PAGE_WRITE | EPT_PAGE_EXECUTE,
	EPT_MAX_PAGE_PERMISSIONS = EPT_ALL_PERMISSIONS
};

class Ept {
private:
	MTRR_RANGE_DESCRIPTOR memoryRanges[MTRR_ENTRIES_SIZE];
	ULONG numberOfEnabledMemoryRanges;
	EPTP eptPointer;
	PVMM_EPT_PAGE_TABLE eptPageTable;
	UCHAR defaultMemoryType;
	LIST_ENTRY* hookedPages;
	Spinlock hookedPagesLock;
	bool executeOnlySupport;

	_IRQL_requires_max_(APC_LEVEL)
	bool CheckFeatures();

	_IRQL_requires_max_(APC_LEVEL)
	void BuildMtrrMap();

	_IRQL_requires_max_(HIGH_LEVEL)
	PEPT_PML1_ENTRY GetPml1Entry(_In_ SIZE_T physicalAddress);

	_IRQL_requires_max_(HIGH_LEVEL)
	PEPT_PML2_ENTRY GetPml2Entry(_In_ SIZE_T physicalAddress);

	_IRQL_requires_max_(HIGH_LEVEL)
	PVOID GetPml1OrPml2Entry(_In_ SIZE_T physicalAddress, _Inout_ bool* isLargePage);

	_IRQL_requires_max_(HIGH_LEVEL)
	PVMM_EPT_DYNAMIC_SPLIT GetDynamicSplit(_In_ SIZE_T physicalAddress);

	_IRQL_requires_max_(HIGH_LEVEL)
	UCHAR GetMemoryType(_In_ ULONG64 pfn, _In_ bool isLargePage);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool SplitLargePage(_Inout_ PVOID buffer, _In_ SIZE_T physicalAddress, _In_ bool canCoalesce, _Out_opt_ bool* splitCreated = nullptr);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool TryCoalesceDynamicSplit(_Inout_opt_ PVMM_EPT_DYNAMIC_SPLIT split) const;

	_IRQL_requires_max_(HIGH_LEVEL)
	void ClearPendingMtfRestore(_In_ PEPT_HOOKED_PAGE_DETAIL hookedEntry);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool ReleaseHookedPageRecord(_Inout_ PEPT_HOOKED_PAGE_DETAIL hookedEntry);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsValidForLargePage(_In_ ULONG64 pfn);

	_IRQL_requires_max_(APC_LEVEL)
	bool SetupPML2Entry(_Inout_ PEPT_PML2_ENTRY newEntry, _In_ SIZE_T pageFrameNumber);

	_IRQL_requires_max_(APC_LEVEL)
	PVMM_EPT_PAGE_TABLE AllocateAndCreateIdentityPageTable();

	_IRQL_requires_max_(APC_LEVEL)
	bool LogicalProcessorInitialize();

	_IRQL_requires_max_(HIGH_LEVEL)
	bool SetPML1AndInvalidateTLB(_Inout_ PEPT_PML1_ENTRY pml1Entry, _In_ EPT_PML1_ENTRY pml1Value, _In_ INVEPT_TYPE invalidationType);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsAccessFromKernelImage(_In_ UINT64 guestRip) const;

	_IRQL_requires_max_(HIGH_LEVEL)
	bool HandleHookedPage(_Inout_ EPT_HOOKED_PAGE_DETAIL* hookedEntryDetails,
		_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification,
		_In_ ULONG64 guestLinearAddress,
		_In_ ULONG64 guestRip,
		_Out_ bool* restoreHookAfterInstruction);

	_IRQL_requires_max_(HIGH_LEVEL) bool HandlePageHookExit(_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification,
		_In_ UINT64 guestPhysicalAddr,
		_In_ ULONG64 guestLinearAddress,
		_In_ ULONG64 guestRip);

	_IRQL_requires_max_(APC_LEVEL)
	void UnhookAllPages();

	_IRQL_requires_max_(HIGH_LEVEL)
	PEPT_HOOKED_PAGE_DETAIL GetHookedPage(_In_ UINT64 guestVirtualAddress);

	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsHookExists(_In_ UINT64 guestVirtualAddress);

	_IRQL_requires_max_(APC_LEVEL)
	void ReleaseAllHookedPageRecords();

public:
	_IRQL_requires_max_(APC_LEVEL)
	Ept();

	_IRQL_requires_max_(APC_LEVEL)
	~Ept();

	/*
	* Description:
	* operator new allocates nonpaged storage for an EPT instance.
	*
	* Parameters:
	* @size [size_t] -- The number of bytes to allocate.
	*
	* Returns:
	* @address [void*] -- The allocated address, or nullptr on failure.
	*/
	_IRQL_requires_max_(APC_LEVEL) void* operator new(size_t size) {
		return AllocateVirtualMemory<PVOID>(size, false);
	}

	/*
	* Description:
	* operator delete releases storage previously allocated for an EPT instance.
	*
	* Parameters:
	* @p [void*] -- The address to release.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(APC_LEVEL) void operator delete(void* p) {
		FreeVirtualMemory(p);
	}

	_IRQL_requires_max_(HIGH_LEVEL) bool RootModePageHook(_In_ PVOID TargetFunc, _In_ UINT8 permissions);
	_IRQL_requires_max_(HIGH_LEVEL) void HandleEptViolation(_In_ ULONG64 exitQualification, _In_ ULONG64 guestPhysicalAddr);
	_IRQL_requires_max_(HIGH_LEVEL) void HandleMisconfiguration(_In_ UINT64 guestPhysicalAddress);
	_IRQL_requires_max_(HIGH_LEVEL) bool PageUnhookVmcall(_In_ UINT64 guestVirtualAddress);
	_IRQL_requires_max_(HIGH_LEVEL) bool UnhookAllPagesVmcall();
	_IRQL_requires_max_(HIGH_LEVEL) void HandleMonitorTrapFlag(_Inout_ PEPT_MTF_RESTORE_CONTEXT restoreContext);
	_IRQL_requires_max_(HIGH_LEVEL) ULONG64 GetEptPointerFlags() const;
};
