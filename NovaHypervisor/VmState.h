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
	bool IncrementRip;
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
	PEPT_HOOKED_PAGE_DETAIL HookedPage;
	Ept* EptInstance;

	VmState() = default;
	~VmState() = default;

	void* operator new(size_t size) {
		return AllocateVirtualMemory<PVOID>(size, false);
	}
	void operator delete(void* p) {
		FreeVirtualMemory(p);
	}
};

