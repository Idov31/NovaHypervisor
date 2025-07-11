#pragma once

#include "pch.h"
#include "WindowsDefinitions.h"

#define DRIVER_TAG 'avoN'
#define VALID_USERMODE_MEMORY(MemAddress)(MemAddress > 0 && MemAddress < 0x7FFFFFFFFFFFFFFF)

/*
* Description:
* GetPhysicalAddress is responsible for getting the physical address of a virtual address.
*
* Parameters:
* @virtualAddress  [_In_ UINT64] -- Virtual address to get the physical address of.
*
* Returns:
* @physicalAddress [UINT64]		 -- Physical address.
*/
inline UINT64 GetPhysicalAddress(_In_ UINT64 virtualAddress) {
	PHYSICAL_ADDRESS physicalAddress = MmGetPhysicalAddress(reinterpret_cast<PVOID>(virtualAddress));
	return static_cast<UINT64>(physicalAddress.QuadPart);
}

/*
* Description:
* GetVirtualAddress is responsible for getting the virtual address of a physical address.
*
* Parameters:
* @physicalAddress  [_In_ UINT64] -- Physical address to get the virtual address of.
*
* Returns:
* @virtualAddress	[UINT64]	  -- Virtual address.
*/
inline UINT64 GetVirtualAddress(_In_ UINT64 physicalAddress) {
	PHYSICAL_ADDRESS physicalAddr = { 0 };
	physicalAddr.QuadPart = physicalAddress;

	return reinterpret_cast<UINT64>(MmGetVirtualForPhysical(physicalAddr));
}

/*
* Description:
* FreeVirtualMemory is responsible for freeing virtual memory and null it.
*
* Parameters:
* @address [PVOID] -- Address to free.
*
* Returns:
* There is no return value.
*/
inline void FreeVirtualMemory(_In_ PVOID address) {
	if (!address)
		return;
	ExFreePoolWithTag(address, DRIVER_TAG);
	address = NULL;
}

/*
* Description:
* AllocateVirtualMemory is responsible for allocating virtual memory with the right function depends on the windows version.
*
* Parameters:
* @size				    [size_t]	  -- Size to allocate.
* @paged				[bool]		  -- Paged or non-paged.
* @forceDeprecatedAlloc [bool]		  -- Force allocation with ExAllocatePoolWithTag.
*
* Returns:
* @ptr					[PointerType] -- Allocated pointer on success else NULL.
*/
template <typename PointerType>
inline PointerType AllocateVirtualMemory(size_t size, bool paged = true, bool forceDeprecatedAlloc = false) {
	PVOID allocatedMem = NULL;

	if (AllocatePool2 && WindowsBuildNumber >= WIN_2004 && !forceDeprecatedAlloc) {
		allocatedMem = paged ? ((tExAllocatePool2)AllocatePool2)(POOL_FLAG_PAGED, size, DRIVER_TAG) :
			((tExAllocatePool2)AllocatePool2)(POOL_FLAG_NON_PAGED, size, DRIVER_TAG);
	}
	else {
#pragma warning( push )
#pragma warning( disable : 4996)
		allocatedMem = paged ? ExAllocatePoolWithTag(PagedPool, size, DRIVER_TAG) :
			ExAllocatePoolWithTag(NonPagedPool, size, DRIVER_TAG);
#pragma warning( pop )
	}

	if (allocatedMem)
		RtlSecureZeroMemory(allocatedMem, size);
	return reinterpret_cast<PointerType>(allocatedMem);
}

/*
* Description:
* ProbeAddress is responsible for probing an address and returning specific status code on failure.
*
* Parameters:
* @address	   [PVOID]	  -- Address to probe.
* @len		   [SIZE_T]   -- Structure size.
* @alignment   [ULONG]    -- Address' required alignment.
* @failureCode [NTSTATUS] -- Failure code.
*
* Returns:
* @status	   [NTSTATUS] -- NTSUCCESS if succeeded else failure code.
*/
inline NTSTATUS ProbeAddress(PVOID address, SIZE_T len, ULONG alignment, NTSTATUS failureCode) {
	NTSTATUS status = STATUS_SUCCESS;

	if (!VALID_USERMODE_MEMORY((ULONGLONG)address))
		return STATUS_ABANDONED;

	__try {
		ProbeForRead(address, len, alignment);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = failureCode;
	}

	return status;
}