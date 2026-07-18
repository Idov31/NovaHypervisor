#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "MemoryHelper.hpp"

// Forward declarations to avoid circular include dependencies
struct _EPT_HOOKED_PAGE_DETAIL;
typedef struct _EPT_HOOKED_PAGE_DETAIL EPT_HOOKED_PAGE_DETAIL, * PEPT_HOOKED_PAGE_DETAIL;
class Ept;

typedef struct _VMX_VMXOFF_STATE
{
	bool IsVmxoffExecuted;
	UINT64  GuestRip;
	UINT64  GuestRsp;
} VMX_VMXOFF_STATE, * PVMX_VMXOFF_STATE;

class VmState {
public:
	bool IsOnVmxRoot;
	bool IsLaunched;
	UINT64 VmxonRegion;
	UINT64 VmxonRegionPhysical;
	UINT64 VmcsRegion;
	UINT64 VmcsRegionPhysical;
	UINT64 VmmStack;
	UINT64 MsrBitmap;
	UINT64 MsrBitmapPhysical;
	VMX_VMXOFF_STATE VmxoffState;
	PEPT_HOOKED_PAGE_DETAIL HookedPages;
	EPT_MTF_RESTORE_CONTEXT MtfRestore;
	Ept* EptInstance;
	bool IsVmxOn;

	VmState() = default;
	~VmState() = default;

	/*
	* Description:
	* operator new allocates nonpaged storage for a VM state.
	*
	* Parameters:
	* @size [size_t] -- The number of bytes to allocate.
	*
	* Returns:
	* @address [void*] -- The allocated address, or nullptr on failure.
	*/
	_IRQL_requires_max_(APC_LEVEL)
	void* operator new(size_t size) {
		return AllocateVirtualMemory<PVOID>(size, false);
	}
	/*
	* Description:
	* operator delete releases storage previously allocated for a VM state.
	*
	* Parameters:
	* @p [void*] -- The address to release.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(DISPATCH_LEVEL)
	void operator delete(void* p) {
		FreeVirtualMemory(p);
	}
};
