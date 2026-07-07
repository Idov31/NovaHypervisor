#pragma once
#include "pch.h"
#include "GlobalVariables.h"
#include "HypervisorDefinitions.h"
#include "Ept.h"
#include "ComLogger.h"

constexpr int IOCTL_PROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
constexpr int IOCTL_UNPROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);

constexpr auto IsValidSize = [](_In_ size_t dataSize, _In_ size_t structSize) -> bool {
	return dataSize != 0 && dataSize % structSize == 0;
};

constexpr auto VALID_KERNELMODE_MEMORY = [](_In_ size_t address) -> bool {
	return address > 0x8000000000000000 && address < 0xFFFFFFFFFFFFFFFF;
};

typedef struct _IPI_PAGE_OPERATION_CONTEXT {
	HookedPage HookRequest;
	UINT64 Address;
	NTSTATUS* ProcessorStatuses;
	ULONG ProcessorCount;
} IPI_PAGE_OPERATION_CONTEXT, * PIPI_PAGE_OPERATION_CONTEXT;

void InitializeIpiProcessorStatuses(
	_Out_writes_(processorCount) NTSTATUS* processorStatuses, 
	_In_ ULONG processorCount);
void RecordIpiProcessorStatus(_Inout_ PIPI_PAGE_OPERATION_CONTEXT context, _In_ NTSTATUS status);
NTSTATUS AggregateIpiProcessorStatuses(
	_In_reads_(processorCount) NTSTATUS* processorStatuses,
	_In_ ULONG processorCount,
	_In_ const char* operationName);
bool IsKernelImagePage(_In_ UINT64 address);
ULONG_PTR HookPageOnProcessor(_In_ ULONG_PTR context);
ULONG_PTR UnhookPageOnProcessor(_In_ ULONG_PTR context);
DRIVER_DISPATCH NovaDeviceControl;
