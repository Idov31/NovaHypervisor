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

void InitializeDpcProcessorStatuses(
	_Out_writes_(processorCount) NTSTATUS* processorStatuses, 
	_In_ ULONG processorCount);
void SetCurrentProcessorStatus(_In_opt_ PVOID context, _In_ NTSTATUS status);
bool AreProcessorStatusesSuccessful(
	_In_reads_(processorCount) NTSTATUS* processorStatuses,
	_In_ ULONG processorCount,
	_In_ const char* operationName);
bool HasAnyProcessorVmxState(_In_ ULONG processorCount);
void FreeProcessorVmResources(_Inout_ VmState* state);
bool VmxInitialize();

_IRQL_requires_max_(APC_LEVEL)
bool VmxInitializer();

void TerminateVmx();
bool VmxTerminate();

void InitializeGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void TerminateGuest(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void UnhookAllPagesDpc(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
void AllocateVmStructures(_In_ KDPC* dpc, _In_opt_ PVOID deferredContext, _In_opt_ PVOID systemArgument1, _In_opt_ PVOID systemArgument2);
bool AllocateRegion(_In_ RegionType regionType, _Inout_ VmState* state);

void VmxVmxoff();

extern "C" { 
	bool VirtualizeProcessor(_In_ PVOID guestStack); 
	UINT64 GetCurrentGuestRip();
	UINT64 GetCurrentGuestRsp();
}
bool SetupVmcs(_Inout_ VmState* state, _In_ PVOID guestStack);
