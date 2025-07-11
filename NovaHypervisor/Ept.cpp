#include "pch.h"
#include "Ept.h"

Ept::Ept() {
	eptPointer = { 0 };
	numberOfEnabledMemoryRanges = 0;
	eptPageTable = NULL;
	defaultMemoryType = 0xFF;
	executeOnlySupport = true;
	memset(memoryRanges, 0, sizeof(memoryRanges));
	hookedPages = AllocateVirtualMemory<PLIST_ENTRY>(sizeof(LIST_ENTRY), false);

	if (!hookedPages) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate memory for hooked pages list");
		ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
	}
	InitializeListHead(hookedPages);
	hookedPagesLock = Spinlock();

	if (!CheckFeatures()) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "EPT features are not supported by the processor");
		FreeVirtualMemory(hookedPages);
		ExRaiseStatus(STATUS_NOT_SUPPORTED);
	}
	BuildMtrrMap();
	NovaHypervisorLog(TRACE_FLAG_INFO, "MTRR memory map built successfully");

	if (!LogicalProcessorInitialize()) {
		FreeVirtualMemory(hookedPages);
		ExRaiseStatus(STATUS_INSUFFICIENT_RESOURCES);
	}
}

Ept::~Ept() {
	UnhookAllPages();
	FreeVirtualMemory(hookedPages);

	if (this->eptPageTable) {
		MmFreeContiguousMemory(this->eptPageTable);
		this->eptPageTable = NULL;
	}
}

/*
* Description:
* GetEptPointerFlags is responsible for returning the flags member from the EPT pointer.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @flags [ULONG64] -- EPT pointer flags.
*/
ULONG64 Ept::GetEptPointerFlags() const {
	return eptPointer.Flags;
}

/*
* Description:
* CheckFeatures is responsible for checking that all the critical EPT features are supported by the processor.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @supported [bool] -- True if all the critical EPT features are supported, otherwise false.
*/
bool Ept::CheckFeatures() {
	IA32_VMX_EPT_VPID_CAP_REGISTER vpidRegister = { 0 };
	IA32_MTRR_DEF_TYPE_REGISTER mtrrDefType = { 0 };

	vpidRegister.Flags = __readmsr(MSR_IA32_VMX_EPT_VPID_CAP);
	mtrrDefType.Flags = __readmsr(MSR_IA32_MTRR_DEF_TYPE);

	if (!vpidRegister.PageWalkLength4 || !vpidRegister.MemoryTypeWriteBack || !vpidRegister.Pde2MbPages || 
		!mtrrDefType.MtrrEnable) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "EPT is not supported by the processor.");
		return false;
	}

	if (!vpidRegister.AdvancedVmexitEptViolationsInformation)
		NovaHypervisorLog(TRACE_FLAG_WARNING, "The processor doesn't report advanced vmexit information for EPT violations.");

	if (!vpidRegister.ExecuteOnlyPages) {
		executeOnlySupport = false;
		NovaHypervisorLog(TRACE_FLAG_WARNING, "The processor doesn't support execute-only pages.");
	}

	NovaHypervisorLog(TRACE_FLAG_INFO, "All important EPT features are present.");
	return true;
}

/*
* Description:
* BuildMtrrMap is responsible for building the MTRR map.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void Ept::BuildMtrrMap() {
	IA32_MTRR_CAPABILITIES_REGISTER mtrrCapabilities = { 0 };
	IA32_MTRR_PHYSBASE_REGISTER currentPhyiscalBase = { 0 };
	IA32_MTRR_PHYSMASK_REGISTER currentPhysicalMask = { 0 };
	IA32_MTRR_FIXED_RANGE_TYPE k64Types = { 0 };
	IA32_MTRR_FIXED_RANGE_TYPE k16Types = { 0 };
	IA32_MTRR_FIXED_RANGE_TYPE k4Types = { 0 };
	PMTRR_RANGE_DESCRIPTOR descriptor = nullptr;
	ULONG numberOfBitsInMask = 0;
	IA32_MTRR_DEF_TYPE_REGISTER mtrrDefType = { 0 };
	mtrrDefType.Flags = __readmsr(MSR_IA32_MTRR_DEF_TYPE);

	if (!mtrrDefType.MtrrEnable) {
		this->defaultMemoryType = MEMORY_TYPE_UNCACHEABLE;
		return;
	}
	this->defaultMemoryType = static_cast<UCHAR>(mtrrDefType.DefaultMemoryType);
	mtrrCapabilities.Flags = __readmsr(MSR_IA32_MTRR_CAPABILITIES);

	if (mtrrCapabilities.FixedRangeSupported && mtrrDefType.FixedRangeMtrrEnable) {
		k64Types.All = __readmsr(IA32_MTRR_FIX64K_00000);

		for (unsigned int i = 0; i < 8; i++) {
			descriptor = &this->memoryRanges[this->numberOfEnabledMemoryRanges++];
			descriptor->MemoryType = k64Types.s.Types[i];
			descriptor->PhysicalBaseAddress = IA32_MTRR_FIX64K_BASE + (IA32_MTRR_FIX64K_SIZE * i);
			descriptor->PhysicalEndAddress = IA32_MTRR_FIX64K_BASE + (IA32_MTRR_FIX64K_SIZE * i) + (IA32_MTRR_FIX64K_SIZE - 1);
			descriptor->FixedRange = true;
		}

		for (unsigned int i = 0; i < 2; i++) {
			k16Types.All = __readmsr(IA32_MTRR_FIX16K_80000 + i);

			for (unsigned int j = 0; j < 8; j++) {
				descriptor = &this->memoryRanges[this->numberOfEnabledMemoryRanges++];
				descriptor->MemoryType = k16Types.s.Types[j];
				descriptor->PhysicalBaseAddress = (IA32_MTRR_FIX16K_BASE + (i * IA32_MTRR_FIX16K_SIZE * 8)) + (IA32_MTRR_FIX16K_SIZE * j);
				descriptor->PhysicalEndAddress = (IA32_MTRR_FIX16K_BASE + (i * IA32_MTRR_FIX16K_SIZE * 8)) + (IA32_MTRR_FIX16K_SIZE * j) +
					(IA32_MTRR_FIX16K_SIZE - 1);
				descriptor->FixedRange = true;
			}
		}

		for (unsigned int i = 0; i < 8; i++) {
			k4Types.All = __readmsr(IA32_MTRR_FIX4K_C0000 + i);

			for (unsigned int j = 0; j < 8; j++) {
				descriptor = &this->memoryRanges[this->numberOfEnabledMemoryRanges++];
				descriptor->MemoryType = k4Types.s.Types[j];
				descriptor->PhysicalBaseAddress = (IA32_MTRR_FIX4K_BASE + (i * IA32_MTRR_FIX4K_SIZE * 8)) + (IA32_MTRR_FIX4K_SIZE * j);
				descriptor->PhysicalEndAddress = (IA32_MTRR_FIX4K_BASE + (i * IA32_MTRR_FIX4K_SIZE * 8)) + (IA32_MTRR_FIX4K_SIZE * j) +
					(IA32_MTRR_FIX4K_SIZE - 1);
				descriptor->FixedRange = true;
			}
		}
	}

	for (UINT64 currentRegister = 0; currentRegister < mtrrCapabilities.VariableRangeCount; currentRegister++) {
		currentPhyiscalBase.Flags = __readmsr(MSR_IA32_MTRR_PHYSBASE0 + (currentRegister * 2));
		currentPhysicalMask.Flags = __readmsr(MSR_IA32_MTRR_PHYSMASK0 + (currentRegister * 2));

		if (currentPhysicalMask.Valid) {
			descriptor = &this->memoryRanges[this->numberOfEnabledMemoryRanges++];
			descriptor->PhysicalBaseAddress = currentPhyiscalBase.PageFrameNumber * PAGE_SIZE;
			_BitScanForward64(&numberOfBitsInMask, currentPhysicalMask.PageFrameNumber * PAGE_SIZE);
			descriptor->PhysicalEndAddress = descriptor->PhysicalBaseAddress + (1ULL << numberOfBitsInMask) - 1;
			descriptor->MemoryType = (UCHAR)currentPhyiscalBase.Type;
			descriptor->FixedRange = false;

			if (descriptor->MemoryType == MEMORY_TYPE_WRITE_BACK)
				this->numberOfEnabledMemoryRanges--;
			NovaHypervisorLog(TRACE_FLAG_DEBUG, "MTRR Range: Base=0x%llx End=0x%llx Type=0x%x", descriptor->PhysicalBaseAddress, descriptor->PhysicalEndAddress,
				descriptor->MemoryType);
		}
	}
	NovaHypervisorLog(TRACE_FLAG_INFO, "Default memory type: 0x%x", this->defaultMemoryType);
}

/*
* Description:
* GetPml1Entry is responsible for getting a PML1 entry.
*
* Parameters:
* @physicalAddress [_In_ SIZE_T]				 -- The physical address to search.
*
* Returns:
* @pml1			   [PEPT_PML1_ENTRY]			 -- The PML1 entry.
*/
PEPT_PML1_ENTRY Ept::GetPml1Entry(_In_ SIZE_T physicalAddress) {
	SIZE_T directory = ADDRMASK_EPT_PML2_INDEX(physicalAddress);
	SIZE_T directoryPointer = ADDRMASK_EPT_PML3_INDEX(physicalAddress);
	SIZE_T pml4Entry = ADDRMASK_EPT_PML4_INDEX(physicalAddress);

	if (pml4Entry > 0)
		return NULL;
	PEPT_PML2_ENTRY pml2 = &eptPageTable->PML2[directoryPointer][directory];

	if (pml2->LargePage)
		return NULL;
	PEPT_PML1_ENTRY pml1 = reinterpret_cast<PEPT_PML1_ENTRY>(GetVirtualAddress(reinterpret_cast<PEPT_PML2_POINTER>(pml2)->PageFrameNumber * PAGE_SIZE));

	if (!pml1)
		return NULL;
	pml1 = &pml1[ADDRMASK_EPT_PML1_INDEX(physicalAddress)];
	return pml1;
}

/*
* Description:
* GetPml2Entry is responsible for getting a PML2 entry.
*
* Parameters:
* @physicalAddress [_In_ SIZE_T]				 -- The physical address to search.
*
* Returns:
* @pml2			   [PEPT_PML2_ENTRY]			 -- The PML2 entry.
*/
PEPT_PML2_ENTRY Ept::GetPml2Entry(_In_ SIZE_T physicalAddress) {
	SIZE_T directory = ADDRMASK_EPT_PML2_INDEX(physicalAddress);
	SIZE_T directoryPointer = ADDRMASK_EPT_PML3_INDEX(physicalAddress);
	SIZE_T pml4Entry = ADDRMASK_EPT_PML4_INDEX(physicalAddress);

	// Addresses above 512GB are invalid because it is greater than physical address bus width 
	if (pml4Entry > 0)
		return NULL;
	return &eptPageTable->PML2[directoryPointer][directory];
}

/*
* Description:
* SplitLargePage is responsible for splitting a large page into smaller pages.
*
* Parameters:
* @buffer		   [_Inout_ PVOID]				 -- The buffer to store the split.
* @physicalAddress [_In_ SIZE_T]				 -- The physical address to split.
*
* Returns:
* @status		   [bool]						 -- True if splitted else false.
*/
bool Ept::SplitLargePage(_Inout_ PVOID buffer, _In_ SIZE_T physicalAddress) {

	EPT_PML1_ENTRY pml1Template = { 0 };
	EPT_PML2_POINTER newSplitPtr = { 0 };

	if (!buffer) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Buffer is not allocated");
		return false;
	}

	PEPT_PML2_ENTRY pml2Entry = GetPml2Entry(physicalAddress);

	if (!pml2Entry) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "An invalid physical address passed 0x%llx", physicalAddress);
		return false;
	}

	// If this large page is not marked a large page, that means it's already splitted.
	if (!pml2Entry->LargePage) {
		poolManager->Free(buffer, SPLIT_2MB_PAGING_TO_4KB_PAGE);
		return true;
	}
	PVMM_EPT_DYNAMIC_SPLIT newSplit = static_cast<PVMM_EPT_DYNAMIC_SPLIT>(buffer);

	if (!newSplit) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate dynamic split memory");
		return false;
	}
	RtlSecureZeroMemory(newSplit, sizeof(VMM_EPT_DYNAMIC_SPLIT));
	newSplit->Entry = pml2Entry;

	// Make a template for RWX and copy it to all PML1 entries.
	pml1Template.Flags = 0;
	pml1Template.ReadAccess = 1;
	pml1Template.WriteAccess = 1;
	pml1Template.ExecuteAccess = 1;
	pml1Template.MemoryType = pml2Entry->MemoryType;
	pml1Template.IgnorePat = pml2Entry->IgnorePat;
	pml1Template.SuppressVe = pml2Entry->SuppressVe;

	__stosq(reinterpret_cast<SIZE_T*>(&newSplit->PML1[0]), pml1Template.Flags, VMM_EPT_PML1E_COUNT);

	// Set the PFNs for identity mapping by converting the 2MB PFN to 4KB + offset to the frame.
	for (SIZE_T entryIndex = 0; entryIndex < VMM_EPT_PML1E_COUNT; entryIndex++) {
		newSplit->PML1[entryIndex].PageFrameNumber = ((pml2Entry->PageFrameNumber * SIZE_2_MB) / PAGE_SIZE) + entryIndex;
		newSplit->PML1[entryIndex].MemoryType = GetMemoryType(newSplit->PML1[entryIndex].PageFrameNumber, false);
	}

	// Set a new pointer that points to the new split instead of the 2MB page.
	newSplitPtr.Flags = 0;
	newSplitPtr.WriteAccess = 1;
	newSplitPtr.ReadAccess = 1;
	newSplitPtr.ExecuteAccess = 1;
	newSplitPtr.PageFrameNumber = GetPhysicalAddress(reinterpret_cast<UINT64>(&newSplit->PML1[0])) / PAGE_SIZE;
	RtlCopyMemory(pml2Entry, &newSplitPtr, sizeof(newSplitPtr));

	return true;
}

/*
* Description:
* SetupPML2Entry is responsible for setting the correct memory type for a PML2 entry.
*
* Parameters:
* @newEntry		   [_Inout_ PEPT_PML2_ENTRY] -- The new PML2 entry.
* @pageFrameNumber [_In_ SIZE_T]			 -- The page frame number.
*
* Returns:
* @status		   [bool]					-- True if the entry is set up successfully, otherwise false.
*/
bool Ept::SetupPML2Entry(_Inout_ PEPT_PML2_ENTRY newEntry, _In_ SIZE_T pageFrameNumber) {
	newEntry->PageFrameNumber = pageFrameNumber;
	SIZE_T addressOfPage = pageFrameNumber * SIZE_2_MB;

	if (IsValidForLargePage(pageFrameNumber)) {
		newEntry->MemoryType = GetMemoryType(pageFrameNumber, true);
		return true;
	}
	// First entry MUST be uncacheable, otherwise it will cause a page fault.
	if (pageFrameNumber == 0) {
		newEntry->MemoryType = MEMORY_TYPE_UNCACHEABLE;
		return true;
	}
	PVOID buffer = poolManager->Allocate(SPLIT_2MB_PAGING_TO_4KB_PAGE);

	if (buffer)
		return SplitLargePage(buffer, pageFrameNumber * SIZE_2_MB);
	return false;
}

/*
* Description:
* AllocateAndCreateIdentityPageTable is responsible for creating a new EPT page table.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @pageTable [PVMM_EPT_PAGE_TABLE] -- The allocated page table.
*/
PVMM_EPT_PAGE_TABLE Ept::AllocateAndCreateIdentityPageTable() {
	PHYSICAL_ADDRESS maxPhysicalAddress = { 0 };
	EPT_PML3_POINTER pml3Template = { 0 };
	EPT_PML2_ENTRY pml2Template = { 0 };
	maxPhysicalAddress.QuadPart = MAXULONG64;

	PVMM_EPT_PAGE_TABLE pageTable = static_cast<PVMM_EPT_PAGE_TABLE>(MmAllocateContiguousMemory(sizeof(VMM_EPT_PAGE_TABLE), maxPhysicalAddress));

	if (!pageTable) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate memory for EPT page table");
		return NULL;
	}
	RtlSecureZeroMemory(pageTable, sizeof(VMM_EPT_PAGE_TABLE));

	// Mark the first 512GB PML4 entry as present to manage up to 512GB of discrete paging structures. 
	pageTable->PML4[0].PageFrameNumber = GetPhysicalAddress(reinterpret_cast<UINT64>(&pageTable->PML3[0])) / PAGE_SIZE;
	pageTable->PML4[0].ReadAccess = 1;
	pageTable->PML4[0].WriteAccess = 1;
	pageTable->PML4[0].ExecuteAccess = 1;

	// Copy RWX template to all PML3 entries.
	pml3Template.Flags = 0;
	pml3Template.ReadAccess = 1;
	pml3Template.WriteAccess = 1;
	pml3Template.ExecuteAccess = 1;
	__stosq(reinterpret_cast<SIZE_T*>(&pageTable->PML3[0]), pml3Template.Flags, VMM_EPT_PML3E_COUNT);

	// For each of the 512 PML3 entries 
	for (SIZE_T entryIndex = 0; entryIndex < VMM_EPT_PML3E_COUNT; entryIndex++)
		pageTable->PML3[entryIndex].PageFrameNumber = GetPhysicalAddress(reinterpret_cast<UINT64>(&pageTable->PML2[entryIndex][0])) / PAGE_SIZE;

	// Copy RWX template to all PML2 entries and mark them as present.
	pml2Template.Flags = 0;
	pml2Template.WriteAccess = 1;
	pml2Template.ReadAccess = 1;
	pml2Template.ExecuteAccess = 1;
	pml2Template.LargePage = 1;
	__stosq(reinterpret_cast<SIZE_T*>(&pageTable->PML2[0]), pml2Template.Flags, VMM_EPT_PML3E_COUNT * VMM_EPT_PML2E_COUNT);

	for (SIZE_T entryGroupIndex = 0; entryGroupIndex < VMM_EPT_PML3E_COUNT; entryGroupIndex++) {
		for (SIZE_T entryIndex = 0; entryIndex < VMM_EPT_PML2E_COUNT; entryIndex++) {
			if (!SetupPML2Entry(&pageTable->PML2[entryGroupIndex][entryIndex], (entryGroupIndex * VMM_EPT_PML2E_COUNT) + entryIndex)) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to setup PML2 entry for page table");
				MmFreeContiguousMemory(pageTable);
				pageTable = NULL;
				return NULL;
			}
		}
	}
	return pageTable;
}

/*
* Description:
* LogicalProcessorInitialize is responsible for initializing the EPT.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @status [bool] -- True if the EPT is initialized, otherwise false.
*/
bool Ept::LogicalProcessorInitialize() {
	EPTP eptp = { 0 };
	PVMM_EPT_PAGE_TABLE pageTable = AllocateAndCreateIdentityPageTable();

	if (!pageTable) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Unable to allocate memory for EPT");
		return false;
	}
	this->eptPageTable = pageTable;

	eptp.Flags = 0;
	eptp.MemoryType = MEMORY_TYPE_WRITE_BACK;
	eptp.EnableAccessAndDirtyFlags = FALSE;

	// Must be 3, indicating an EPT page-walk length of 4.
	eptp.PageWalkLength = 3;
	eptp.PageFrameNumber = GetPhysicalAddress(reinterpret_cast<UINT64>(&pageTable->PML4)) / PAGE_SIZE;
	this->eptPointer = eptp;
	return true;
}

/*
* Description:
* HandlePageHookExit is responsible for handling a page hook vmexit.
*
* Parameters:
* @violationQualification [_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION] -- The EPT violation qualification.
* @guestPhysicalAddr	  [_In_ UINT64]								  -- The guest physical address.
*
* Returns:
* @status				  [bool]									  -- True if the page hook exit is handled, otherwise false.
*/
_Use_decl_annotations_
bool Ept::HandlePageHookExit(_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification, _In_ UINT64 guestPhysicalAddr) {
	PEPT_HOOKED_PAGE_DETAIL hookedEntry = NULL;
	bool handled = false;
	UINT64 alignedPhysicalAddress = reinterpret_cast<UINT64>(PAGE_ALIGN(guestPhysicalAddr));

	if (!alignedPhysicalAddress) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Target address could not be mapped to physical memory");
		return handled;
	}
	UINT64 virtualAddress = GetVirtualAddress(guestPhysicalAddr);
	this->hookedPagesLock.Lock();
	PLIST_ENTRY currentEntry = this->hookedPages;

	while (currentEntry->Flink != this->hookedPages) {
		currentEntry = currentEntry->Flink;
		hookedEntry = CONTAINING_RECORD(currentEntry, EPT_HOOKED_PAGE_DETAIL, Entry);

		if (hookedEntry->PhysicalBaseAddress == alignedPhysicalAddress) {
			ULONG currentProcessor = KeGetCurrentProcessorNumber();
			handled = HandleHookedPage(hookedEntry, violationQualification, virtualAddress);

			if (handled && !GuestState[currentProcessor].IncrementRip) {
				GuestState[currentProcessor].HookedPage = hookedEntry;
				VmxHelper::SetMonitorTrapFlag(true);
			}
			break;
		}
	}
	
	this->hookedPagesLock.Unlock();
	return handled;
}

/*
* Description:
* HandleHookedPage is responsible to handle a hooked page.
*
* Parameters:
* @hookedEntryDetails	  [_Inout_ EPT_HOOKED_PAGE_DETAIL]			  -- The hooked page details.
* @violationQualification [_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION] -- The violation qualification.
* @guestVirtualAddress    [_In_ ULONG64]							  -- The guest virtual address that caused the hook.
*
* Returns:
* @status				  [bool]									  -- True if the page is handled, otherwise false.
*/
bool Ept::HandleHookedPage(_Inout_ EPT_HOOKED_PAGE_DETAIL* hookedEntryDetails,
	_In_ VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification, _In_ ULONG64 guestVirtualAddress) {
	bool operationAllowed = false;
	bool handled = false;
	PEPT_PML1_ENTRY pml1Entry = hookedEntryDetails->EntryAddress;

	if (!violationQualification.EptExecutable && violationQualification.ExecuteAccess) {
		if (guestVirtualAddress >= KernelBaseInfo.KernelBaseAddress &&
			guestVirtualAddress < (KernelBaseInfo.KernelBaseAddress + KernelBaseInfo.KernelSize)) {
			pml1Entry->ExecuteAccess = 1;
			operationAllowed = true;
			NovaHypervisorLog(TRACE_FLAG_INFO, "Allowed execute access to protected address from: 0x%llx", guestVirtualAddress);
		}
		else
			NovaHypervisorLog(TRACE_FLAG_INFO, "Blocked execute access to protected address from: 0x%llx", guestVirtualAddress);
		handled = true;
	}
	else if (!violationQualification.EptWriteable && violationQualification.WriteAccess) {
		if (guestVirtualAddress >= KernelBaseInfo.KernelBaseAddress &&
			guestVirtualAddress < (KernelBaseInfo.KernelBaseAddress + KernelBaseInfo.KernelSize)) {
			pml1Entry->WriteAccess = 1;
			operationAllowed = true;
			NovaHypervisorLog(TRACE_FLAG_INFO, "Allowed write access to protected address from: 0x%llx", guestVirtualAddress);
		}
		else
			NovaHypervisorLog(TRACE_FLAG_INFO, "Blocked write access to protected address from: 0x%llx", guestVirtualAddress);
		handled = true;
	}
	else if (!violationQualification.EptReadable && violationQualification.ReadAccess) {
		if (guestVirtualAddress >= KernelBaseInfo.KernelBaseAddress &&
			guestVirtualAddress < (KernelBaseInfo.KernelBaseAddress + KernelBaseInfo.KernelSize)) {
			pml1Entry->ReadAccess = 1;
			operationAllowed = true;
			NovaHypervisorLog(TRACE_FLAG_INFO, "Allowed read access to protected address from: 0x%llx", guestVirtualAddress);
		}
		else
			NovaHypervisorLog(TRACE_FLAG_INFO, "Blocked read access to protected address from: 0x%llx", guestVirtualAddress);
		handled = true;
	}

	if (operationAllowed) {
		SetPML1AndInvalidateTLB(hookedEntryDetails->EntryAddress, hookedEntryDetails->OriginalEntry, SINGLE_CONTEXT);
		GuestState[KeGetCurrentProcessorNumber()].IncrementRip = false;
	}
	return handled;
}

/*
* Description:
* HandleEptViolation is responsible for handling ept violations.
*
* Parameters:
* @exitQualification [_In_ ULONG64] -- The exit qualification.
* @guestPhysicalAddr [_In_ ULONG64] -- The guest physical address.
*
* Returns:
* There is no return value.
*/
void Ept::HandleEptViolation(_In_ ULONG64 exitQualification, _In_ ULONG64 guestPhysicalAddr) {
	VMX_EXIT_QUALIFICATION_EPT_VIOLATION violationQualification = { 0 };
	violationQualification.Flags = exitQualification;

	if (HandlePageHookExit(violationQualification, guestPhysicalAddr))
		return;

	NovaHypervisorLog(TRACE_FLAG_ERROR, "Unexpected EPT violation at 0x%llx", guestPhysicalAddr);
	DbgBreakPoint();
}

/*
* Description:
* HandleMisconfiguration is responsible for handling EPT misconfigurations.
*
* Parameters:
* @guestAddress [_In_ UINT64] -- The guest address.
*
* Returns:
* There is no return value.
*/
void Ept::HandleMisconfiguration(_In_ UINT64 guestAddress) {
	NovaHypervisorLog(TRACE_FLAG_ERROR, "EPT Misconfiguration!");
	NovaHypervisorLog(TRACE_FLAG_ERROR, "A field in the EPT paging structure was invalid, faulting guest address: 0x%llx", guestAddress);
	DbgBreakPoint();
}

/*
* Description:
* HandleMonitorTrapFlag is responsible for handling MTF event.
*
* Parameters:
* @hookedEntry [_Inout_ PEPT_HOOKED_PAGE_DETAIL] -- The hooked page detail.
*
* Returns:
* There is no return value.
*/
void Ept::HandleMonitorTrapFlag(_Inout_ PEPT_HOOKED_PAGE_DETAIL hookedEntry) {
	SetPML1AndInvalidateTLB(hookedEntry->EntryAddress, hookedEntry->ChangedEntry, SINGLE_CONTEXT);
	NovaHypervisorLog(TRACE_FLAG_INFO, "Restored hooked page 0x%llx", hookedEntry->VirtualAddress);
}

/*
* Description:
* RootModePageHook is responsible for hooking a page in VMX root mode.
*
* Parameters:
* @targetFunc	[_In_ PVOID] -- The target function to hook.
* @permissions [_In_ UINT8]  -- The permissions for the page.
*
* Returns:
* @status		[bool]	     -- True if the page is hooked, otherwise false.
*/
bool Ept::RootModePageHook(_In_ PVOID targetFunc, _In_ UINT8 permissions) {
	EPT_PML1_ENTRY changedEntry = { 0 };
	ULONG currentProcessorIndex = KeGetCurrentProcessorIndex();

	if (GuestState[currentProcessorIndex].IsOnVmxRoot && !GuestState[currentProcessorIndex].IsLaunched)
		return false;

	if (permissions & EPT_PAGE_WRITE && !(permissions & EPT_PAGE_READ)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Invalid permissions to perform a hook");
		return false;
	}

	if (IsHookExists(reinterpret_cast<UINT64>(targetFunc))) {
		NovaHypervisorLog(TRACE_FLAG_INFO, "Hook already exists for the target function: 0x%llx", reinterpret_cast<UINT64>(targetFunc));
		return true;
	}

	PVOID virtualFuncAddress = PAGE_ALIGN(targetFunc);
	SIZE_T physicalFuncAddress = GetPhysicalAddress(reinterpret_cast<UINT64>(virtualFuncAddress));

	if (!physicalFuncAddress) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Target address could not be mapped to physical memory");
		return false;
	}
	PVOID targetBuffer = poolManager->Allocate(SPLIT_2MB_PAGING_TO_4KB_PAGE);

	if (!targetBuffer) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate memory for the target buffer");
		return false;
	}

	if (!SplitLargePage(targetBuffer, physicalFuncAddress)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Could not split page for the address: 0x%llx", physicalFuncAddress);
		poolManager->Free(targetBuffer, SPLIT_2MB_PAGING_TO_4KB_PAGE);
		return false;
	}
	PEPT_PML1_ENTRY pml1Entry = GetPml1Entry(physicalFuncAddress);

	if (!pml1Entry) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to get PML1 entry of the target address: 0x%llx", physicalFuncAddress);
		poolManager->Free(targetBuffer, SPLIT_2MB_PAGING_TO_4KB_PAGE);
		return false;
	}
	changedEntry.Flags = pml1Entry->Flags;
	changedEntry.ReadAccess = permissions & EPT_PAGE_READ;
	changedEntry.WriteAccess = permissions & EPT_PAGE_WRITE;
	changedEntry.ExecuteAccess = permissions & EPT_PAGE_EXECUTE;

	PEPT_HOOKED_PAGE_DETAIL hookedEntry = static_cast<PEPT_HOOKED_PAGE_DETAIL>(poolManager->Allocate(EPT_HOOK_PAGE));

	if (!hookedEntry) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to allocate memory for the hooked entry");
		poolManager->Free(targetBuffer, SPLIT_2MB_PAGING_TO_4KB_PAGE);
		return false;
	}
	hookedEntry->IsExecutionHook = false;
	hookedEntry->VirtualAddress = reinterpret_cast<UINT64>(targetFunc);
	hookedEntry->PhysicalAddress = physicalFuncAddress;
	hookedEntry->PhysicalBaseAddress = reinterpret_cast<UINT64>(PAGE_ALIGN(physicalFuncAddress));
	hookedEntry->EntryAddress = pml1Entry;
	hookedEntry->OriginalEntry = *pml1Entry;
	hookedEntry->ChangedEntry = changedEntry;
	this->hookedPagesLock.Lock();
	InsertHeadList(this->hookedPages, &(hookedEntry->Entry));
	this->hookedPagesLock.Unlock();

	// Invalidate the entry in the TLB caches so it will not conflict with the actual paging structure.
	if (GuestState[currentProcessorIndex].IsLaunched)
		SetPML1AndInvalidateTLB(pml1Entry, changedEntry, SINGLE_CONTEXT);
	else
		pml1Entry->Flags = changedEntry.Flags;
	return true;
}

/*
* Description:
* PageHook is responsible for hooking a page.
*
* Parameters:
* @targetFunc	[_In_ PVOID] -- The target function to hook.
* @permissions  [_In_ UINT8] -- The permissions for the page.
*
* Returns:
* @status		[bool]	     -- True if the page is hooked, otherwise false.
*/
bool Ept::PageHook(_In_ PVOID targetFunc, _In_ UINT8 permissions) {
	ULONG currentProcessor = KeGetCurrentProcessorIndex();

	if (GuestState[currentProcessor].IsLaunched) {
		if (NT_SUCCESS(AsmVmxVmcall(VMCALL_EXEC_HOOK_PAGE, reinterpret_cast<UINT64>(targetFunc), permissions, NULL))) {
			NovaHypervisorLog(TRACE_FLAG_INFO, "Hook applied from vmx root mode");
			KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(VmxHelper::InvalidateEptByVmcall), this->eptPointer.Flags);
			return true;
		}
	}
	else {
		if (RootModePageHook(targetFunc, permissions)) {
			NovaHypervisorLog(TRACE_FLAG_INFO, "Hook applied (vm not launched yet)");
			return true;
		}
	}

	NovaHypervisorLog(TRACE_FLAG_WARNING, "Hook not applied");
	return false;
}

/*
* Description:
* GetMemoryType is responsible for getting a memory type for PFN.
*
* Parameters:
* @pfn				[_In_ ULONG64] -- The PFN.
* @isLargePage		[_In_ bool]	   -- True if the page is a large page, otherwise false.
*
* Returns:
* @targetMemoryType [UCHAR]		   -- The memory type.
*/
UCHAR Ept::GetMemoryType(_In_ ULONG64 pfn, _In_ bool isLargePage) {
	UCHAR targetMemoryType = 0xFF;
	MTRR_RANGE_DESCRIPTOR* currentMemoryRange;
	ULONG64 addressOfPage = isLargePage ? pfn * SIZE_2_MB : pfn * PAGE_SIZE;

	for (UINT32 currentMtrrRange = 0; currentMtrrRange < this->numberOfEnabledMemoryRanges; currentMtrrRange++) {
		currentMemoryRange = &this->memoryRanges[currentMtrrRange];

		if (addressOfPage >= currentMemoryRange->PhysicalBaseAddress &&
			addressOfPage < currentMemoryRange->PhysicalEndAddress) {
			if (currentMemoryRange->FixedRange) {
				targetMemoryType = currentMemoryRange->MemoryType;
				break;
			}

			if (targetMemoryType == MEMORY_TYPE_UNCACHEABLE) {
				targetMemoryType = currentMemoryRange->MemoryType;
				break;
			}

			if (targetMemoryType == MEMORY_TYPE_WRITE_THROUGH || currentMemoryRange->MemoryType == MEMORY_TYPE_WRITE_THROUGH) {
				if (targetMemoryType == MEMORY_TYPE_WRITE_BACK) {
					targetMemoryType = MEMORY_TYPE_WRITE_THROUGH;
					continue;
				}
			}
			targetMemoryType = currentMemoryRange->MemoryType;
		}
	}

	if (targetMemoryType == 0xFF)
		targetMemoryType = this->defaultMemoryType;

	return targetMemoryType;
}

/*
* Description:
* GetPml1OrPml2Entry is responsible for getting PML1 or PML2 entry.
*
* Parameters:
* @physicalAddress [_In_ SIZE_T]			  -- The physical address to search.
* @isLargePage	   [_Inout_ bool]			  -- True if the page is a large page, otherwise false.
*
* Returns:
* @entry		   [PVOID]					  -- The PML1 or PML2 entry.
*/
PVOID Ept::GetPml1OrPml2Entry(_In_ SIZE_T physicalAddress, _Inout_ bool* isLargePage) {
	SIZE_T directory = ADDRMASK_EPT_PML2_INDEX(physicalAddress);
	SIZE_T directoryPointer = ADDRMASK_EPT_PML3_INDEX(physicalAddress);
	SIZE_T pml4Entry = ADDRMASK_EPT_PML4_INDEX(physicalAddress);

	if (pml4Entry > 0)
		return NULL;
	PEPT_PML2_ENTRY pml2 = &eptPageTable->PML2[directoryPointer][directory];

	if (pml2->LargePage) {
		*isLargePage = true;
		return pml2;
	}
	PEPT_PML2_POINTER pml2Pointer = reinterpret_cast<PEPT_PML2_POINTER>(pml2);
	PEPT_PML1_ENTRY pml1 = reinterpret_cast<PEPT_PML1_ENTRY>(GetVirtualAddress(pml2Pointer->PageFrameNumber * PAGE_SIZE));

	if (!pml1)
		return NULL;
	pml1 = &pml1[ADDRMASK_EPT_PML1_INDEX(physicalAddress)];

	*isLargePage = false;
	return pml1;
}

/*
* Description:
* IsValidForLargePage checks if the page is valid for a large page.
*
* Parameters:
* @pfn	  [_In_ ULONG64] -- The page frame number.
*
* Returns:
* @status [bool]		 -- True if the page is valid for a large page, otherwise false.
*/
bool Ept::IsValidForLargePage(_In_ ULONG64 pfn) {
	ULONG64 startPageAddress = pfn * SIZE_2_MB;
	ULONG64 endPageAddress = startPageAddress + SIZE_2_MB - 1;
	MTRR_RANGE_DESCRIPTOR* currentMemoryRange = nullptr;

	for (UINT32 memoryPageIndex = 0; memoryPageIndex < this->numberOfEnabledMemoryRanges; memoryPageIndex++) {
		currentMemoryRange = &this->memoryRanges[memoryPageIndex];

		if ((startPageAddress <= currentMemoryRange->PhysicalEndAddress && endPageAddress > currentMemoryRange->PhysicalEndAddress) ||
			(startPageAddress < currentMemoryRange->PhysicalBaseAddress && endPageAddress >= currentMemoryRange->PhysicalBaseAddress))
			return false;
	}
	return true;
}

/*
* Description:
* SetPML1AndInvalidateTLB is responsible for setting the PML1 entry and invalidating the TLB.
*
* Parameters:
* @pml1Entry	    [_Inout_ PEPT_PML1_ENTRY] -- The PML1 entry to set.
* @pml1Value	    [_In_ EPT_PML1_ENTRY]	  -- The PML1 value to set.
* @invalidationType [_In_ INVEPT_TYPE]		  -- The invalidation type.
*
* Returns:
* There is no return value.
*/
_Use_decl_annotations_
void Ept::SetPML1AndInvalidateTLB(_Inout_ PEPT_PML1_ENTRY pml1Entry, _In_ EPT_PML1_ENTRY pml1Value, _In_ INVEPT_TYPE invalidationType) {
	pml1Entry->Flags = pml1Value.Flags;

	switch (invalidationType) {
	case SINGLE_CONTEXT:
		VmxHelper::InvalidateEpt(this->eptPointer.Flags);
		break;
	case ALL_CONTEXTS:
		VmxHelper::InvalidateEpt();
		break;
	default:
		break;
	}
}

/*
* Description:
* PageUnhook is responsible to dispatch a vmcall to remove a hooked page.
*
* Parameters:
* @guestVirtualAddress [_In_ UINT64] -- The guest virtual address.
*
* Returns:
* @status			   [bool]		 -- True if the page is unhooked, otherwise false.
*/
bool Ept::PageUnhook(_In_ UINT64 guestVirtualAddress) {
	if (GuestState[KeGetCurrentProcessorNumber()].IsOnVmxRoot)
		return false;
	PEPT_HOOKED_PAGE_DETAIL hookedEntry = GetHookedPage(guestVirtualAddress);

	if (hookedEntry) {
		KeGenericCallDpc(UnhookSinglePage, reinterpret_cast<PVOID>(hookedEntry->VirtualAddress));
		RemoveEntryList(hookedEntry->Entry.Flink);
		return true;
	}
	return false;
}

/*
* Description:
* PageUnhookVmcall is responsible to invalidate the TLB for a specific page.
*
* Parameters:
* @guestPhysicalAddress [_In_ UINT64] -- The guest virtual address.
*
* Returns:
* @status			    [bool]		  -- True if the page is unhooked, otherwise false.
*/
bool Ept::PageUnhookVmcall(_In_ UINT64 guestVirtualAddress) {
	if (!GuestState[KeGetCurrentProcessorNumber()].IsOnVmxRoot)
		return false;
	PEPT_HOOKED_PAGE_DETAIL hookedEntry = GetHookedPage(guestVirtualAddress);

	if (hookedEntry) {
		SetPML1AndInvalidateTLB(hookedEntry->EntryAddress, hookedEntry->OriginalEntry, SINGLE_CONTEXT);
		return true;
	}
	return false;
}

/*
* Description:
* PageUnhook is responsible to unhook all pages.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @status [bool] -- True if unhooked all pages, else false.
*/
bool Ept::UnhookAllPagesVmcall() {
	PEPT_HOOKED_PAGE_DETAIL hookedEntry = nullptr;

	if (!GuestState[KeGetCurrentProcessorNumber()].IsOnVmxRoot)
		return false;
	PLIST_ENTRY entry = this->hookedPages;

	while (this->hookedPages != entry->Flink) {
		entry = entry->Flink;
		hookedEntry = CONTAINING_RECORD(entry, EPT_HOOKED_PAGE_DETAIL, Entry);
		SetPML1AndInvalidateTLB(hookedEntry->EntryAddress, hookedEntry->OriginalEntry, SINGLE_CONTEXT);
	}
	return true;
}

/*
* Description:
* PageUnhook is responsible to dispatch a vmcall to unhook all pages.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
void Ept::UnhookAllPages() {
	if (GuestState[KeGetCurrentProcessorNumber()].IsOnVmxRoot)
		return;
	KeGenericCallDpc(UnhookAllPagesDpc, NULL);

	// Clean up the protected addresses
	hookedPagesLock.Lock();
	this->hookedPages->Blink = NULL;
	this->hookedPages->Flink = NULL;
	hookedPagesLock.Unlock();
}

/*
* Description:
*
* IsHookExists checks if a hook exists for the given guest virtual address.
*
* Parameters:
* @guestVirtualAddress [_In_ UINT64] -- The guest virtual address to check.
*
* Returns:
* @status [bool] -- True if a hook exists, otherwise false.
*/
bool Ept::IsHookExists(_In_ UINT64 guestVirtualAddress) {
	return GetHookedPage(guestVirtualAddress) != NULL;
}

/*
* Description:
* GetHookedPage retrieves the hooked page details for a given guest virtual address.
*
* Parameters:
* @guestVirtualAddress [_In_ UINT64] -- The guest virtual address to retrieve the hooked page for.
*
* Returns:
* @hookedPage [PEPT_HOOKED_PAGE_DETAIL] -- The details of the hooked page, or NULL if not found.
*/
PEPT_HOOKED_PAGE_DETAIL Ept::GetHookedPage(_In_ UINT64 guestVirtualAddress) {
	PEPT_HOOKED_PAGE_DETAIL hookedEntry = nullptr;

	this->hookedPagesLock.Lock();
	PLIST_ENTRY entry = this->hookedPages;

	if (IsListEmpty(this->hookedPages)) {
		this->hookedPagesLock.Unlock();
		return NULL;
	}

	while (this->hookedPages != entry->Flink) {
		entry = entry->Flink;
		hookedEntry = CONTAINING_RECORD(entry, EPT_HOOKED_PAGE_DETAIL, Entry);

		if (hookedEntry->VirtualAddress == guestVirtualAddress) {
			this->hookedPagesLock.Unlock();
			return hookedEntry;
		}
	}
	this->hookedPagesLock.Unlock();
	return NULL;
}