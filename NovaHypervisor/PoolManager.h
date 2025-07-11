#pragma once
#include "pch.h"
#include "AutoLock.hpp"
#include "Spinlock.h"
#include "MemoryHelper.hpp"
#include "HypervisorDefinitions.h"
#include "RequestList.hpp"
#include "WppDefinitions.h"
#include "PoolManager.tmh"

constexpr SIZE_T MAX_INITIAL_ALLOCATIONS = 10;
constexpr SIZE_T MAX_WAIT_ITERATIONS = 10;
constexpr UINT64 DEFAULT_SLEEP_MS = 100;

enum ALLOCATION_TYPE {
	SPLIT_2MB_PAGING_TO_4KB_PAGE,
	EPT_HOOK_PAGE
};

typedef struct _FREE_REQUEST {
	PVOID Address;
	ALLOCATION_TYPE Type;
} FREE_REQUEST, * PFREE_REQUEST;

typedef struct _ALLOCATION_REQUEST {
	ALLOCATION_TYPE type;
} ALLOCATION_REQUEST, * PALLOCATION_REQUEST;

typedef struct _POOL_ALLOCATION {
	PVOID Address;
	bool IsUsed;
	ALLOCATION_TYPE Type;
	LIST_ENTRY Entry;
} POOL_ALLOCATION, * PPOOL_ALLOCATION;

typedef struct _ALLOCATION_LIST {
	LIST_ENTRY Head;
	Spinlock Lock;
} ALLOCATION_LIST, * PALLOCATION_LIST;

_IRQL_requires_(PASSIVE_LEVEL)
void ProcessAllocationThread(_In_ PVOID StartContext);
_IRQL_requires_(PASSIVE_LEVEL)
void ProcessFreeThread(_In_ PVOID StartContext);

class PoolManager {
private:
	bool running;
	Spinlock runningLock;
	HANDLE allocationThread;
	HANDLE freeThread;
	ALLOCATION_LIST pagingAllocations;
	ALLOCATION_LIST eptHookAllocations;
	RequestList<ALLOCATION_TYPE> allocationRequests;
	RequestList<POOL_ALLOCATION> freeRequests;

	_IRQL_requires_max_(APC_LEVEL)
	bool AllocateInternal(_In_ ALLOCATION_TYPE type, _In_ bool isInit = false);
	_IRQL_requires_max_(APC_LEVEL)
	void FreeInternal(_In_ PVOID address, _In_ ALLOCATION_TYPE type);
	PVOID FindFreeSlot(_In_ ALLOCATION_TYPE type);
	void StopThreads();

	bool IsValidAllocationType(_In_ ALLOCATION_TYPE type) const {
		return type == SPLIT_2MB_PAGING_TO_4KB_PAGE || type == EPT_HOOK_PAGE;
	}

	void Sleep(_In_ UINT64 milliseconds) const {
		LARGE_INTEGER interval = { 0 };
		interval.QuadPart = -10000 * milliseconds;
		KeDelayExecutionThread(KernelMode, FALSE, &interval);
	}
public:
	void* operator new(size_t size) {
		return AllocateVirtualMemory<PVOID>(size, false);
	}

	void operator delete(void* p) {
		FreeVirtualMemory(p);
	}

	_IRQL_requires_max_(APC_LEVEL)
	PoolManager();
	_IRQL_requires_max_(APC_LEVEL)
	~PoolManager();
	PVOID Allocate(_In_ ALLOCATION_TYPE type);
	void Free(_In_ PVOID address, _In_ ALLOCATION_TYPE type);
	_IRQL_requires_(PASSIVE_LEVEL)
	void ProcessAllocation();
	_IRQL_requires_(PASSIVE_LEVEL)
	void ProcessFree();
};
