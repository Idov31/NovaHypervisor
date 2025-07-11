#include "pch.h"
#include "PoolManager.h"

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

	NTSTATUS status = PsCreateSystemThread(&this->allocationThread, GENERIC_ALL, &objectAttributes, NULL, NULL, 
		(PKSTART_ROUTINE)&ProcessAllocationThread, this);

	if (!NT_SUCCESS(status))
		ExRaiseStatus(status);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Started allocation thread.");

	running = true;
	status = PsCreateSystemThread(&this->freeThread, GENERIC_ALL, &objectAttributes, NULL, NULL,
		(PKSTART_ROUTINE)&ProcessFreeThread, this);

	if (!NT_SUCCESS(status))
		ExRaiseStatus(status);
	NovaHypervisorLog(TRACE_FLAG_DEBUG, "Started free thread.");
}

_IRQL_requires_max_(APC_LEVEL)
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

			// Since allocation->Address is already freed at TerminateVmx we don't need to free it again.
			FreeVirtualMemory(allocation);
			entry = nextEntry;
		}
	};

	FreeAllocations(&pagingAllocations.Head, pagingAllocations.Lock);
	FreeAllocations(&eptHookAllocations.Head, eptHookAllocations.Lock);
}

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
	allocation->Type = type;
	allocation->IsUsed = !isInit;

	InitializeListHead(&allocation->Entry);
	InsertTailList(head, &allocation->Entry);
	return true;
}

_IRQL_requires_max_(APC_LEVEL)
void PoolManager::FreeInternal(_In_ PVOID address, _In_ ALLOCATION_TYPE type) {
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;

	switch (type) {
	case SPLIT_2MB_PAGING_TO_4KB_PAGE:
		head = &pagingAllocations.Head;
		break;
	case EPT_HOOK_PAGE:
		head = &eptHookAllocations.Head;
		break;
	default:
		return;
	}
	Spinlock& lock = (type == SPLIT_2MB_PAGING_TO_4KB_PAGE) ?
		pagingAllocations.Lock : eptHookAllocations.Lock;
	AutoLock<Spinlock> autoLock(lock);

	for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
		allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);

		if (allocation->Address == address) {
			FreeVirtualMemory(allocation->Address);
			allocation->IsUsed = false;
			break;
		}
	}
}

/*
* Description:
* FindFreeSlot is responsible for asking to free memory allocation by pushing it to the free requests queue.
*
* Parameters:
* @address [_In_ PVOID]			  -- The address to free.
* @type	   [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* There is no return value.
*/
PVOID PoolManager::FindFreeSlot(_In_ ALLOCATION_TYPE type) {
	PPOOL_ALLOCATION allocation = nullptr;
	PLIST_ENTRY head = nullptr;
	Spinlock lock;

	switch (type) {
	case SPLIT_2MB_PAGING_TO_4KB_PAGE:
		head = &pagingAllocations.Head;
		lock = pagingAllocations.Lock;
		break;
	case EPT_HOOK_PAGE:
		head = &eptHookAllocations.Head;
		lock = eptHookAllocations.Lock;
		break;
	default:
		return nullptr;
	}
	AutoLock<Spinlock> autoLock(lock);

	for (PLIST_ENTRY entry = head->Flink; entry != head; entry = entry->Flink) {
		allocation = CONTAINING_RECORD(entry, POOL_ALLOCATION, Entry);

		if (!allocation->IsUsed) {
			allocation->IsUsed = true;
			return allocation->Address;
		}
	}
	return allocation;
}

/*
* Description:
* Allocate is responsible for asking for an allocation by pushing it to the free requests queue.
*
* Parameters:
* @type		  [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* @allocation [PVOID]				 -- The allocated memory if allocated else nullptr.
*/
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
* Free is responsible for asking to free memory allocation by pushing it to the free requests queue.
*
* Parameters:
* @address [_In_ PVOID]			  -- The address to free.
* @type	   [_In_ ALLOCATION_TYPE] -- The type of the allocation.
*
* Returns:
* There is no return value.
*/
void PoolManager::Free(_In_ PVOID address, _In_ ALLOCATION_TYPE type) {
	bool queued = false;
	UINT64 waitCount = 0;
	UINT32 wait = 1;
	POOL_ALLOCATION allocation = { 0 };

	if (!address || !IsValidAllocationType(type))
		return;

	allocation.Address = address;
	allocation.Type = type;

	queued = freeRequests.Insert(allocation);

	if (!queued) {
		do {
			for (UINT32 i = 0; i < wait; i++)
				_mm_pause();
			queued = freeRequests.Insert(allocation);

			wait = wait * 2 > MAX_WAIT ? MAX_WAIT : wait * 2;

			if (wait == MAX_WAIT)
				waitCount++;
		} while (!queued && waitCount < MAX_WAIT_ITERATIONS);
	}
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
