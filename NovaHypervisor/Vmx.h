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
#include "ComLogger.h"

typedef struct _PROCESSOR_DPC_STATUS_CONTEXT {
	NTSTATUS* ProcessorStatuses;
	ULONG ProcessorCount;
} PROCESSOR_DPC_STATUS_CONTEXT, * PPROCESSOR_DPC_STATUS_CONTEXT;

_IRQL_requires_max_(APC_LEVEL)
void InitializeDpcProcessorStatuses(
	_Out_writes_(processorCount) NTSTATUS* processorStatuses, 
	_In_ ULONG processorCount);
_IRQL_requires_(DISPATCH_LEVEL)
void SetCurrentProcessorStatus(_In_opt_ PVOID context, _In_ NTSTATUS status);
_IRQL_requires_max_(APC_LEVEL)
bool AreProcessorStatusesSuccessful(
	_In_reads_(processorCount) NTSTATUS* processorStatuses,
	_In_ ULONG processorCount,
	_In_ const char* operationName);
_IRQL_requires_max_(APC_LEVEL)
bool HasAnyProcessorVmxState(_In_ ULONG processorCount);
_IRQL_requires_max_(DISPATCH_LEVEL)
void FreeProcessorVmResources(_Inout_ VmState* state);
_IRQL_requires_max_(APC_LEVEL)
bool VmxInitialize();

_IRQL_requires_max_(APC_LEVEL)
bool VmxInitializer();

_IRQL_requires_max_(APC_LEVEL)
void TerminateVmx();
_IRQL_requires_(DISPATCH_LEVEL)
bool VmxTerminate();

_IRQL_requires_(DISPATCH_LEVEL)
void InitializeGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
_IRQL_requires_(DISPATCH_LEVEL)
void TerminateGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
_IRQL_requires_(DISPATCH_LEVEL)
void UnhookAllPagesDpc(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
_IRQL_requires_(DISPATCH_LEVEL)
void AllocateVmStructures(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
_IRQL_requires_(DISPATCH_LEVEL)
bool AllocateRegion(_In_ RegionType regionType, _Inout_ VmState* state);

_IRQL_requires_max_(HIGH_LEVEL)
void VmxVmxoff();

extern "C" { 
	_IRQL_requires_(DISPATCH_LEVEL)
	bool VirtualizeProcessor(_In_ PVOID guestStack); 
	_IRQL_requires_max_(HIGH_LEVEL)
	UINT64 GetCurrentGuestRip();
	_IRQL_requires_max_(HIGH_LEVEL)
	UINT64 GetCurrentGuestRsp();
}
_IRQL_requires_(DISPATCH_LEVEL)
bool SetupVmcs(_Inout_ VmState* state, _In_ PVOID guestStack);
