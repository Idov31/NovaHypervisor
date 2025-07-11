#pragma once
#include "pch.h"
#include "Ept.h"
#include "HypervisorDefinitions.h"
#include "WindowsDefinitions.h"
#include "InlineAsm.h"
#include "GlobalVariables.h"
#include "VmxHelper.h"
#include "MemoryHelper.hpp"
#include "VmState.h"
#include "WppDefinitions.h"
#include "Vmx.tmh"

bool VmxInitialize();
_IRQL_requires_max_(APC_LEVEL)
bool VmxInitializer();

void TerminateVmx();
bool VmxTerminate();

void InitializeGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void TerminateGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void UnhookSinglePage(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void UnhookAllPagesDpc(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
bool AllocateVmStructures(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
bool AllocateRegion(_In_ RegionType regionType, _Inout_ VmState* state);

void VmxVmxoff();

extern "C" { 
	bool VirtualizeProcessor(_In_ PVOID guestStack); 
	UINT64 GetCurrentGuestRip();
	UINT64 GetCurrentGuestRsp();
}
bool SetupVmcs(_Inout_ VmState* state, _In_ PVOID guestStack);
