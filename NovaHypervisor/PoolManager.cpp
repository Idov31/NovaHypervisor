#include "pch.h"
#include "PoolManager.h"

/*
* Description:
* PoolManager initializes the allocation pools and starts their worker threads.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(APC_LEVEL)
PoolManager::PoolManager() {
	OBJECT_ATTRIBUTES objectAttributes{};
	running = false;
	this->allocationThread = NULL;
	this->freeThread = NULL;
	runningLock = Spinlock();
	pagingAllocations.Lock = Spinlock();
	eptHookAllocations.Lock = Spinlock();
	allocationRequests = RequestList<ALLOCATION_TYPE>();
	freeRequests = RequestList<POOL_ALLOCATION>();

	InitializeObjectAttributes(&objectAttributes, NULL, OBJ_KERNEL_HANDLE, NULL, NULL);
	InitializeListHead(&pagingAllocations.Head);
	InitializeListHead(&eptHookAllocations.Head);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Initialized allocation lists.");

	for (USHORT i = 0; i < MAX_INITIAL_ALLOCATIONS; i++) {
		AllocateInternal(SPLIT_2MB_PAGING_TO_4KB_PAGE, true);
		AllocateInternal(EPT_HOOK_PAGE, true);
	}
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Allocated initial blocks.");

	runningLock.Lock();
	running = true;
	runningLock.Unlock();

	NTSTATUS status = PsCreateSystemThread(&this->allocationThread, GENERIC_ALL, &objectAttributes, NULL, NULL, 
		(PKSTART_ROUTINE)&ProcessAllocationThread, this);

	if (!NT_SUCCESS(status)) {
		runningLock.Lock();
		running = false;
		runningLock.Unlock();
		ExRaiseStatus(status);
	}
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Started allocation thread.");

	status = PsCreateSystemThread(&this->freeThread, GENERIC_ALL, &objectAttributes, NULL, NULL,
		(PKSTART_ROUTINE)&ProcessFreeThread, this);

	if (!NT_SUCCESS(status)) {
		StopThreads();
		ExRaiseStatus(status);
	}
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Started free thread.");
}

/*
* Description:
* ~PoolManager stops the worker threads and releases all managed allocations.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
PoolManager::~PoolManager() {
	StopThreads();
	
	auto FreeAllocations = [](_Inout_ PLIST_ENTRY head, _In_ Spinlock& lock) -> void {
		PPOOL_ALLOCATION allocation = nullptr;
		AutoLock<Spinlock> listLock(lock);
		PLIST_ENTRY entry = head->Flink;

		while (entry != head) {
			PLIST_ENTRY nextEntry = entry->Flink;
			allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);
			RemoveEntryList(entry);

			if (allocation->Address)
				FreeVirtualMemory(allocation->Address);
			FreeVirtualMemory(allocation);
			entry = nextEntry;
		}
	};

	FreeAllocations(&pagingAllocations.Head, pagingAllocations.Lock);
	FreeAllocations(&eptHookAllocations.Head, eptHookAllocations.Lock);
}

/*
* Description:
* AllocateInternal adds one allocation to the requested pool.
*
* Parameters:
* @type   [_In_ ALLOCATION_TYPE] -- The allocation type to create.
* @isInit [_In_ bool]            -- Whether the allocation is part of initial setup.
*
* Returns:
* @allocated [bool] -- True if the allocation was created, otherwise false.
*/
_IRQL_requires_max_(APC_LEVEL)
bool PoolManager::AllocateInternal(_In_ ALLOCATION_TYPE type, _In_ bool isInit) {
	SIZE_T allocationSize = 0;
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;

	switch (type) {
		case SPLIT_2MB_PAGING_TO_4KB_PAGE:
			head = &pagingAllocations.Head;
			allocationSize = sizeof(VMM_EPT_DYNAMIC_SPLIT);
			break;
		case EPT_HOOK_PAGE:
			head = &eptHookAllocations.Head;
			allocationSize = sizeof(EPT_HOOKED_PAGE_DETAIL);
			break;
		default:
			return false;
	}
	Spinlock& lock = (type == SPLIT_2MB_PAGING_TO_4KB_PAGE) ?
		pagingAllocations.Lock : eptHookAllocations.Lock;
	AutoLock<Spinlock> autoLock(lock);
	allocation = AllocateVirtualMemory<PPOOL_ALLOCATION>(sizeof(POOL_ALLOCATION), false);

	if (!allocation)
		return false;

	// For some reason, when allocating with ExAllocatePool2 it doesn't work so need to use "legacy" allocation.
	allocation->Address = AllocateVirtualMemory<PVOID>(allocationSize, false, true);

	if (!allocation->Address) {
		FreeVirtualMemory(allocation);
		return false;
	}
	UNREFERENCED_PARAMETER(isInit);
	allocation->Type = type;
	allocation->IsUsed = false;

	InitializeListHead(&allocation->Entry);
	InsertTailList(head, &allocation->Entry);
	return true;
}

/*
* Description:
* FreeInternal clears and returns an allocation to its pool.
*
* Parameters:
* @address [_In_ PVOID]           -- The allocation address to return.
* @type    [_In_ ALLOCATION_TYPE] -- The allocation type.
*
* Returns:
* @freed [bool] -- True if the allocation was returned, otherwise false.
*/
_IRQL_requires_max_(HIGH_LEVEL)
bool PoolManager::FreeInternal(_In_ PVOID address, _In_ ALLOCATION_TYPE type) {
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;
	SIZE_T allocationSize = 0;

	switch (type) {
	case SPLIT_2MB_PAGING_TO_4KB_PAGE:
		head = &pagingAllocations.Head;
		allocationSize = sizeof(VMM_EPT_DYNAMIC_SPLIT);
		break;
	case EPT_HOOK_PAGE:
		head = &eptHookAllocations.Head;
		allocationSize = sizeof(EPT_HOOKED_PAGE_DETAIL);
		break;
	default:
		return false;
	}
	Spinlock& lock = (type == SPLIT_2MB_PAGING_TO_4KB_PAGE) ?
		pagingAllocations.Lock : eptHookAllocations.Lock;
	AutoLock<Spinlock> autoLock(lock);

	for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
		allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);

		if (allocation->Address == address) {
			RtlSecureZeroMemory(allocation->Address, allocationSize);
			allocation->IsUsed = false;
			return true;
		}
	}
	return false;
}

/*
* Description:
* FindFreeSlot is responsible for finding and reserving an unused allocation in a pool list.
*
* Parameters:
* @type	   [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* @slot	   [PVOID]				  -- The reserved allocation, or nullptr when no free slot exists.
*/
_IRQL_requires_max_(HIGH_LEVEL)
PVOID PoolManager::FindFreeSlot(_In_ ALLOCATION_TYPE type) {
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;
	Spinlock* lock = nullptr;

	switch (type) {
	case SPLIT_2MB_PAGING_TO_4KB_PAGE:
		head = &pagingAllocations.Head;
		lock = &pagingAllocations.Lock;
		break;
	case EPT_HOOK_PAGE:
		head = &eptHookAllocations.Head;
		lock = &eptHookAllocations.Lock;
		break;
	default:
		return nullptr;
	}
	AutoLock<Spinlock> autoLock(*lock);

	for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
		allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);

		if (!allocation->IsUsed) {
			allocation->IsUsed = true;
			return allocation->Address;
		}
	}
	return nullptr;
}

/*
* Description:
* CountFreeSlots counts the unused allocations in the requested pool.
*
* Parameters:
* @type [_In_ ALLOCATION_TYPE] -- The allocation type to count.
*
* Returns:
* @count [UINT64] -- The number of unused allocations.
*/
_IRQL_requires_max_(HIGH_LEVEL)
UINT64 PoolManager::CountFreeSlots(_In_ ALLOCATION_TYPE type) {
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;
	Spinlock* lock = nullptr;
	UINT64 freeSlots = 0;

	switch (type) {
	case SPLIT_2MB_PAGING_TO_4KB_PAGE:
		head = &pagingAllocations.Head;
		lock = &pagingAllocations.Lock;
		break;
	case EPT_HOOK_PAGE:
		head = &eptHookAllocations.Head;
		lock = &eptHookAllocations.Lock;
		break;
	default:
		return 0;
	}
	AutoLock<Spinlock> autoLock(*lock);

	for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
		allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);

		if (!allocation->IsUsed)
			freeSlots++;
	}
	return freeSlots;
}

/*
* Description:
* Allocate is responsible for finding a free allocation or queueing background growth when none is available.
*
* Parameters:
* @type		  [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* @allocation [PVOID]				 -- The allocated memory if allocated else nullptr.
*/
_IRQL_requires_max_(HIGH_LEVEL)
PVOID PoolManager::Allocate(_In_ ALLOCATION_TYPE type) {
	bool queued = false;
	UINT64 waitCount = 0;
	UINT32 wait = 1;

	if (!IsValidAllocationType(type))
		return nullptr;
	PVOID allocation = FindFreeSlot(type);

	if (allocation)
		return allocation;

	// Queuing the allocation request.
	queued = allocationRequests.Insert(type);

	if (!queued) {
		do {
			for (UINT32 i = 0; i < wait; i++)
				_mm_pause();
			queued = allocationRequests.Insert(type);

			wait = wait * 2 > MAX_WAIT ? MAX_WAIT : wait * 2;

			if (wait == MAX_WAIT)
				waitCount++;
		} while (!queued && waitCount < MAX_WAIT_ITERATIONS);

		if (waitCount == MAX_WAIT_ITERATIONS)
			return nullptr;
		waitCount = 0;
		wait = 1;
	}

	// Waiting it to be processed.
	do {
		for (UINT32 i = 0; i < wait; i++)
			_mm_pause();
		allocation = FindFreeSlot(type);

		wait = wait * 2 > MAX_WAIT ? MAX_WAIT : wait * 2;

		if (wait == MAX_WAIT)
			waitCount++;
	} while (!allocation && waitCount < MAX_WAIT_ITERATIONS);
	return allocation;
}

/*
* Description:
* TryAllocate reserves an immediately available allocation without queueing pool growth.
*
* Parameters:
* @type [_In_ ALLOCATION_TYPE] -- The allocation type to reserve.
*
* Returns:
* @allocation [PVOID] -- The reserved allocation, or nullptr if none is available.
*/
_IRQL_requires_max_(HIGH_LEVEL)
PVOID PoolManager::TryAllocate(_In_ ALLOCATION_TYPE type) {
	if (!IsValidAllocationType(type))
		return nullptr;
	return FindFreeSlot(type);
}

/*
* Description:
* EnsureFreeSlots grows a pool until it contains the requested number of free allocations.
*
* Parameters:
* @type              [_In_ ALLOCATION_TYPE] -- The allocation type to grow.
* @requiredFreeSlots [_In_ UINT64]          -- The minimum number of free allocations.
*
* Returns:
* @available [bool] -- True if the requested capacity is available, otherwise false.
*/
_IRQL_requires_max_(APC_LEVEL)
bool PoolManager::EnsureFreeSlots(_In_ ALLOCATION_TYPE type, _In_ UINT64 requiredFreeSlots) {
	if (!IsValidAllocationType(type))
		return false;

	while (CountFreeSlots(type) < requiredFreeSlots) {
		if (!AllocateInternal(type))
			return false;
	}
	return true;
}

/*
* Description:
* Free is responsible for synchronously returning an allocation to its pool list.
*
* Parameters:
* @address [_In_ PVOID]			  -- The address to free.
* @type	   [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* @status [bool]				  -- True if the allocation was returned to the pool.
*/
_IRQL_requires_max_(HIGH_LEVEL)
bool PoolManager::Free(_In_ PVOID address, _In_ ALLOCATION_TYPE type) {
	if (!address || !IsValidAllocationType(type))
		return false;
	return FreeInternal(address, type);
}

/*
* Description:
* ProcessAllocation is responsible for periodically allocate memory.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
void PoolManager::ProcessAllocation() {
	ALLOCATION_TYPE type;
	bool isRunning = true;

	auto UpdateRunningState = [&]() -> void {
		runningLock.Lock();
		isRunning = running;
		runningLock.Unlock();
	};
	
	while (isRunning) {
		if (allocationRequests.GetListSize() == 0) {
			Sleep(DEFAULT_SLEEP_MS);
			UpdateRunningState();
			continue;
		}
		type = allocationRequests.Pop();
		AllocateInternal(type);
		UpdateRunningState();
	}
}

/*
* Description:
* ProcessFree is responsible for periodically freeing memory.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
void PoolManager::ProcessFree() {
	bool isRunning = true;
	POOL_ALLOCATION allocation = { 0 };

	auto UpdateRunningState = [&]() -> void {
		runningLock.Lock();
		isRunning = running;
		runningLock.Unlock();
	};

	while (isRunning) {
		if (freeRequests.GetListSize() == 0) {
			Sleep(DEFAULT_SLEEP_MS);
			UpdateRunningState();
			continue;
		}
		allocation = freeRequests.Pop();

		if (allocation.Address)
			FreeInternal(allocation.Address, allocation.Type);
		UpdateRunningState();
	}

	// Free all the remaining allocations.
	if (freeRequests.GetListSize() > 0) {
		while (freeRequests.GetListSize() > 0) {
			allocation = freeRequests.Pop();

			if (allocation.Address)
				FreeInternal(allocation.Address, allocation.Type);
		}
	}
}

/*
* Description:
* StopThreads is responsible for stopping the allocation and free threads.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
void PoolManager::StopThreads() {
	auto StopThread = [](HANDLE thread) -> void {
		NTSTATUS status = STATUS_SUCCESS;

		if (thread) {
			do {
				status = ZwWaitForSingleObject(thread, FALSE, NULL);
			} while (status == STATUS_TIMEOUT);
			ZwClose(thread);
		}
	};
	runningLock.Lock();

	if (running) {
		running = false;

		// Need to explicitly unlock here to prevent deadlock.
		runningLock.Unlock();
		StopThread(allocationThread);
		allocationThread = NULL;
		StopThread(freeThread);
		freeThread = NULL;
	}
	else {
		runningLock.Unlock();
	}
}

/*
* Description:
* ProcessAllocationThread is responsible for periodically allocate memory.
*
* Parameters:
* @StartContext [_In_ PVOID] -- The context that holds the lock, if suppose to continue to run and interval.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
void ProcessAllocationThread(_In_ PVOID StartContext) {
	PoolManager* poolManager = reinterpret_cast<PoolManager*>(StartContext);
	poolManager->ProcessAllocation();
}

/*
* Description:
* ProcessFreeThread is responsible for periodically free memory.
*
* Parameters:
* @StartContext [_In_ PVOID] -- The context that holds the lock, if suppose to continue to run and interval.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(PASSIVE_LEVEL)
void ProcessFreeThread(_In_ PVOID StartContext) {
	PoolManager* poolManager = reinterpret_cast<PoolManager*>(StartContext);
	poolManager->ProcessFree();
}
