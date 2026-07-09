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

	bool CheckFeatures();
	void BuildMtrrMap();
	PEPT_PML1_ENTRY GetPml1Entry(_In_ SIZE_T physicalAddress);
	PEPT_PML2_ENTRY GetPml2Entry(_In_ SIZE_T physicalAddress);
	PVOID GetPml1OrPml2Entry(_In_ SIZE_T physicalAddress, _Inout_ bool* isLargePage);
	PVMM_EPT_DYNAMIC_SPLIT GetDynamicSplit(_In_ SIZE_T physicalAddress);
	UCHAR GetMemoryType(_In_ ULONG64 pfn, _In_ bool isLargePage);
	bool SplitLargePage(_Inout_ PVOID buffer, _In_ SIZE_T physicalAddress, _In_ bool canCoalesce, _Out_opt_ bool* splitCreated = nullptr);
	bool TryCoalesceDynamicSplit(_Inout_ PVMM_EPT_DYNAMIC_SPLIT split) const;
	void ClearPendingMtfRestore(_In_ PEPT_HOOKED_PAGE_DETAIL hookedEntry);
	bool ReleaseHookedPageRecord(_Inout_ PEPT_HOOKED_PAGE_DETAIL hookedEntry);
	bool IsValidForLargePage(_In_ ULONG64 pfn);
	bool SetupPML2Entry(_Inout_ PEPT_PML2_ENTRY newEntry, _In_ SIZE_T pageFrameNumber);
	PVMM_EPT_PAGE_TABLE AllocateAndCreateIdentityPageTable();
	bool LogicalProcessorInitialize();
	bool SetPML1AndInvalidateTLB(_Inout_ PEPT_PML1_ENTRY pml1Entry, _In_ EPT_PML1_ENTRY pml1Value, _In_ INVEPT_TYPE invalidationType);
	bool IsAccessFromKernelImage(_In_ UINT64 guestRip) const;
	bool HandleHookedPage(_Inout_ EPT_HOOKED_PAGE_DETAIL* hookedEntryDetails,
		_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification,
		_In_ ULONG64 guestLinearAddress,
		_In_ ULONG64 guestRip,
		_Out_ bool* restoreHookAfterInstruction);
	bool HandlePageHookExit(_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification,
		_In_ UINT64 guestPhysicalAddr,
		_In_ ULONG64 guestLinearAddress,
		_In_ ULONG64 guestRip);
	void UnhookAllPages();
	PEPT_HOOKED_PAGE_DETAIL GetHookedPage(_In_ UINT64 guestVirtualAddress);
	bool IsHookExists(_In_ UINT64 guestVirtualAddress);
	void ReleaseAllHookedPageRecords();

public:
	Ept();
	~Ept();

	void* operator new(size_t size) {
		return AllocateVirtualMemory<PVOID>(size, false);
	}

	void operator delete(void* p) {
		FreeVirtualMemory(p);
	}

	bool RootModePageHook(_In_ PVOID TargetFunc, _In_ UINT8 permissions);
	void HandleEptViolation(_In_ ULONG64 exitQualification, _In_ ULONG64 guestPhysicalAddr);
	void HandleMisconfiguration(_In_ UINT64 guestPhysicalAddress);
	bool PageUnhookVmcall(_In_ UINT64 guestVirtualAddress);
	bool UnhookAllPagesVmcall();
	void HandleMonitorTrapFlag(_Inout_ PEPT_MTF_RESTORE_CONTEXT restoreContext);
	ULONG64 GetEptPointerFlags() const;
};
