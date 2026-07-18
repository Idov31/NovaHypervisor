#include "pch.h"
#include "DeviceControl.h"

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
_Function_class_(DRIVER_DISPATCH)
_IRQL_requires_max_(DISPATCH_LEVEL)
_IRQL_requires_same_
NTSTATUS NovaDeviceControl(_In_ PDEVICE_OBJECT DeviceObject, _Inout_ PIRP Irp) {
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
			InitializeIpiProcessorStatuses(processorStatuses, processorCount);
			IPI_PAGE_OPERATION_CONTEXT operationContext = { hookedPage, hookedPage.Address, processorStatuses, processorCount };

			KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(HookPageOnProcessor), reinterpret_cast<ULONG_PTR>(&operationContext));
			status = AggregateIpiProcessorStatuses(processorStatuses, processorCount, "Hook page");

			if (!NT_SUCCESS(status)) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to hook page 0x%llx with permissions 0x%x (0x%08X)",
					hookedPage.Address, hookedPage.Permissions, status);
				InitializeIpiProcessorStatuses(processorStatuses, processorCount);
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
			InitializeIpiProcessorStatuses(processorStatuses, processorCount);
			IPI_PAGE_OPERATION_CONTEXT operationContext = { { 0 }, protectedMemory, processorStatuses, processorCount };

			KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(UnhookPageOnProcessor), reinterpret_cast<ULONG_PTR>(&operationContext));
			status = AggregateIpiProcessorStatuses(processorStatuses, processorCount, "Unhook page");
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

/*
* Description:
* InitializeIpiProcessorStatuses is responsible for initializing each processor status before running an IPI operation.
*
* Parameters:
* @processorStatuses [_Out_writes_(processorCount) NTSTATUS*] -- The processor status array to initialize.
* @processorCount	 [_In_ ULONG]							  -- The number of entries in the status array.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(APC_LEVEL)
void InitializeIpiProcessorStatuses(
	_Out_writes_(processorCount) NTSTATUS* processorStatuses,
	_In_ ULONG processorCount) {
	for (ULONG i = 0; i < processorCount; i++)
		processorStatuses[i] = STATUS_UNSUCCESSFUL;
}

/*
* Description:
* RecordIpiProcessorStatus is responsible for storing the current processor result in the IPI operation context.
*
* Parameters:
* @context [_Inout_ PIPI_PAGE_OPERATION_CONTEXT] -- The IPI operation context.
* @status  [_In_ NTSTATUS]					   -- The status to store for the current processor.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_(IPI_LEVEL)
void RecordIpiProcessorStatus(_Inout_ PIPI_PAGE_OPERATION_CONTEXT context, _In_ NTSTATUS status) {
	if (!context || !context->ProcessorStatuses)
		return;
	ULONG currentProcessor = KeGetCurrentProcessorNumber();

	if (currentProcessor < context->ProcessorCount)
		context->ProcessorStatuses[currentProcessor] = status;
}

/*
* Description:
* AggregateIpiProcessorStatuses is responsible for converting per-processor IPI results into a single status.
*
* Parameters:
* @processorStatuses [_In_reads_(processorCount) NTSTATUS*] -- The status reported by each processor.
* @processorCount	 [_In_ ULONG]						   -- The number of processor status entries.
* @operationName	 [_In_ const char*]					   -- The operation name to include in failure logs.
*
* Returns:
* @status			 [NTSTATUS]							   -- STATUS_SUCCESS if all processors succeeded, otherwise the first failure status.
*/
_IRQL_requires_max_(APC_LEVEL)
NTSTATUS AggregateIpiProcessorStatuses(
	_In_reads_(processorCount) NTSTATUS* processorStatuses,
	_In_ ULONG processorCount,
	_In_ const char* operationName) {
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

/*
* Description:
* IsKernelImagePage is responsible for checking whether an address belongs to the loaded kernel image.
*
* Parameters:
* @address [_In_ UINT64] -- The virtual address to check.
*
* Returns:
* @status  [bool]		  -- True if the page is fully inside the kernel image, otherwise false.
*/
_IRQL_requires_max_(DISPATCH_LEVEL)
bool IsKernelImagePage(_In_ UINT64 address) {
	const UINT64 kernelBase = KernelBaseInfo.KernelBaseAddress;
	const UINT64 kernelSize = KernelBaseInfo.KernelSize;

	if (!VALID_KERNELMODE_MEMORY(address) || !kernelBase || !kernelSize || kernelBase + kernelSize < kernelBase)
		return false;
	UINT64 pageBase = reinterpret_cast<UINT64>(PAGE_ALIGN(address));
	UINT64 pageEnd = pageBase + PAGE_SIZE;

	if (pageEnd < pageBase)
		return false;
	return pageBase >= kernelBase && pageEnd <= kernelBase + kernelSize;
}

/*
* Description:
* HookPageOnProcessor is responsible for applying a page hook on the current processor through a VMCALL.
*
* Parameters:
* @context [_In_ ULONG_PTR] -- The IPI operation context.
*
* Returns:
* @status  [ULONG_PTR]	   -- The NTSTATUS result returned as an ULONG_PTR for KeIpiGenericCall.
*/
_IRQL_requires_(IPI_LEVEL)
ULONG_PTR HookPageOnProcessor(_In_ ULONG_PTR context) {
	PIPI_PAGE_OPERATION_CONTEXT operationContext = reinterpret_cast<PIPI_PAGE_OPERATION_CONTEXT>(context);
	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (operationContext) {
		status = VmxHelper::HookPageByVmcall(reinterpret_cast<UINT64>(&operationContext->HookRequest));
		RecordIpiProcessorStatus(operationContext, status);
	}
	return static_cast<ULONG_PTR>(status);
}

/*
* Description:
* UnhookPageOnProcessor is responsible for removing a page hook on the current processor through a VMCALL.
*
* Parameters:
* @context [_In_ ULONG_PTR] -- The IPI operation context.
*
* Returns:
* @status  [ULONG_PTR]	   -- The NTSTATUS result returned as an ULONG_PTR for KeIpiGenericCall.
*/
_IRQL_requires_(IPI_LEVEL)
ULONG_PTR UnhookPageOnProcessor(_In_ ULONG_PTR context) {
	PIPI_PAGE_OPERATION_CONTEXT operationContext = reinterpret_cast<PIPI_PAGE_OPERATION_CONTEXT>(context);
	NTSTATUS status = STATUS_INVALID_PARAMETER;

	if (operationContext) {
		status = VmxHelper::UnhookPageByVmcall(operationContext->Address);
		RecordIpiProcessorStatus(operationContext, status);
	}
	return static_cast<ULONG_PTR>(status);
}