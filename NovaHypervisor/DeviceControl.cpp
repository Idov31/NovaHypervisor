#include "pch.h"
#include "DeviceControl.h"

static void InitializeProcessorStatuses(_Out_writes_(processorCount) NTSTATUS* processorStatuses, _In_ ULONG processorCount) {
	for (ULONG i = 0; i < processorCount; i++)
		processorStatuses[i] = STATUS_UNSUCCESSFUL;
}

static void RecordProcessorStatus(_Inout_ PIPI_PAGE_OPERATION_CONTEXT context, _In_ NTSTATUS status) {
	if (!context || !context->ProcessorStatuses)
		return;
	ULONG currentProcessor = KeGetCurrentProcessorNumber();

	if (currentProcessor < context->ProcessorCount)
		context->ProcessorStatuses[currentProcessor] = status;
}

static NTSTATUS AggregateProcessorStatuses(_In_reads_(processorCount) NTSTATUS* processorStatuses, _In_ ULONG processorCount, _In_ const char* operationName) {
	NTSTATUS aggregateStatus = STATUS_SUCCESS;

	for (ULONG i = 0; i < processorCount; i++) {
		if (!NT_SUCCESS(processorStatuses[i])) {
			NovaHypervisorLog(TRACE_FLAG_ERROR, "%s failed on processor %d: 0x%08X", operationName, i, processorStatuses[i]);

			if (NT_SUCCESS(aggregateStatus))
				aggregateStatus = processorStatuses[i];
		}
	}
	return aggregateStatus;
}

static bool IsKernelImagePage(_In_ UINT64 address) {
	const UINT64 kernelBase = KernelBaseInfo.KernelBaseAddress;
	const UINT64 kernelSize = KernelBaseInfo.KernelSize;

	if (!kernelBase || !kernelSize || kernelBase + kernelSize < kernelBase)
		return false;
	UINT64 pageBase = reinterpret_cast<UINT64>(PAGE_ALIGN(address));
	UINT64 pageEnd = pageBase + PAGE_SIZE;

	if (pageEnd < pageBase)
		return false;
	return pageBase >= kernelBase && pageEnd <= kernelBase + kernelSize;
}

static ULONG_PTR HookPageOnProcessor(_In_ ULONG_PTR context) {
	PIPI_PAGE_OPERATION_CONTEXT operationContext = reinterpret_cast<PIPI_PAGE_OPERATION_CONTEXT>(context);
	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (operationContext) {
		status = VmxHelper::HookPageByVmcall(reinterpret_cast<UINT64>(&operationContext->HookRequest));
		RecordProcessorStatus(operationContext, status);
	}
	return static_cast<ULONG_PTR>(status);
}

static ULONG_PTR UnhookPageOnProcessor(_In_ ULONG_PTR context) {
	PIPI_PAGE_OPERATION_CONTEXT operationContext = reinterpret_cast<PIPI_PAGE_OPERATION_CONTEXT>(context);
	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (operationContext) {
		status = VmxHelper::UnhookPageByVmcall(operationContext->Address);
		RecordProcessorStatus(operationContext, status);
	}
	return static_cast<ULONG_PTR>(status);
}

/*
* Description:
* NovaDeviceControl is responsible for handling IOCTLs.
*
* Parameters:
* @DeviceObject [PDEVICE_OBJECT] -- Not used.
* @Irp			[PIRP]			 -- The IRP that contains the user data such as SystemBuffer, Irp stack, etc.
*
* Returns:
* @status		[NTSTATUS]		 -- Always will be STATUS_SUCCESS.
*/
NTSTATUS NovaDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	NTSTATUS status = STATUS_SUCCESS;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG size = stack->Parameters.DeviceIoControl.InputBufferLength;

	switch (stack->Parameters.DeviceIoControl.IoControlCode) {
		case IOCTL_PROTECT_ADDRESS_RANGE: {
			if (size != sizeof(HookedPage)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			HookedPage hookedPage = *static_cast<HookedPage*>(Irp->AssociatedIrp.SystemBuffer);
			PVOID alignedAddress = PAGE_ALIGN(reinterpret_cast<PVOID>(hookedPage.Address));

			if (!VALID_KERNELMODE_MEMORY(hookedPage.Address) ||
				hookedPage.Permissions == 0 ||
				hookedPage.Permissions > EPT_MAX_PAGE_PERMISSIONS ||
				((hookedPage.Permissions & EPT_PAGE_WRITE) && !(hookedPage.Permissions & EPT_PAGE_READ)) ||
				!MmIsNonPagedSystemAddressValid(alignedAddress) ||
				!IsKernelImagePage(hookedPage.Address)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			ULONG processorCount = KeQueryActiveProcessorCount(0);

			if (!poolManager ||
				!poolManager->EnsureFreeSlots(SPLIT_2MB_PAGING_TO_4KB_PAGE, processorCount) ||
				!poolManager->EnsureFreeSlots(EPT_HOOK_PAGE, processorCount)) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			NTSTATUS* processorStatuses = AllocateVirtualMemory<NTSTATUS*>(sizeof(NTSTATUS) * processorCount, false);

			if (!processorStatuses) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			InitializeProcessorStatuses(processorStatuses, processorCount);
			IPI_PAGE_OPERATION_CONTEXT operationContext = { hookedPage, hookedPage.Address, processorStatuses, processorCount };

			KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(HookPageOnProcessor), reinterpret_cast<ULONG_PTR>(&operationContext));
			status = AggregateProcessorStatuses(processorStatuses, processorCount, "Hook page");

			if (!NT_SUCCESS(status)) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to hook page 0x%llx with permissions 0x%x (0x%08X)",
					hookedPage.Address, hookedPage.Permissions, status);
				InitializeProcessorStatuses(processorStatuses, processorCount);
				KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(UnhookPageOnProcessor), reinterpret_cast<ULONG_PTR>(&operationContext));
				FreeVirtualMemory(processorStatuses);
				break;
			}
			FreeVirtualMemory(processorStatuses);
			NovaHypervisorLog(TRACE_FLAG_INFO, "Hooked page 0x%llx with permissions 0x%x",
				hookedPage.Address, hookedPage.Permissions);
			break;
		}
		case IOCTL_UNPROTECT_ADDRESS_RANGE: {
			if (size != sizeof(UINT64) || !Irp->AssociatedIrp.SystemBuffer) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			UINT64 protectedMemory = *static_cast<UINT64*>(Irp->AssociatedIrp.SystemBuffer);

			if (!VALID_KERNELMODE_MEMORY(protectedMemory) ||
				!IsKernelImagePage(protectedMemory)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			ULONG processorCount = KeQueryActiveProcessorCount(0);
			NTSTATUS* processorStatuses = AllocateVirtualMemory<NTSTATUS*>(sizeof(NTSTATUS) * processorCount, false);

			if (!processorStatuses) {
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}
			InitializeProcessorStatuses(processorStatuses, processorCount);
			IPI_PAGE_OPERATION_CONTEXT operationContext = { { 0 }, protectedMemory, processorStatuses, processorCount };

			KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(UnhookPageOnProcessor), reinterpret_cast<ULONG_PTR>(&operationContext));
			status = AggregateProcessorStatuses(processorStatuses, processorCount, "Unhook page");
			FreeVirtualMemory(processorStatuses);
			if (!NT_SUCCESS(status))
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to unhook page 0x%llx (0x%08X)", protectedMemory, status);
			break;
		}
		default: {
			status = STATUS_INVALID_DEVICE_REQUEST;
			break;
		}
	}

	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}
