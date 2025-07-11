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
			HookedPage* hookedPage = static_cast<HookedPage*>(Irp->AssociatedIrp.SystemBuffer);

			if (!VALID_KERNELMODE_MEMORY(hookedPage->Address) ||
				hookedPage->Permissions > EPT_MAX_PAGE_PERMISSIONS ||
				hookedPage->Permissions & EPT_PAGE_WRITE && !(hookedPage->Permissions & EPT_PAGE_READ)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = static_cast<NTSTATUS>(KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(VmxHelper::HookPageByVmcall), 
				reinterpret_cast<UINT64>(hookedPage)));

			if (!NT_SUCCESS(status)) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to hook page 0x%llx with permissions 0x%llx (0x%08X)",
					hookedPage->Address, hookedPage->Permissions, status);
				break;
			}
			NovaHypervisorLog(TRACE_FLAG_INFO, "Hooked page 0x%llx with permissions 0x%llx",
				hookedPage->Address, hookedPage->Permissions);
			break;
		}
		case IOCTL_UNPROTECT_ADDRESS_RANGE: {
			if (size != sizeof(UINT64)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			UINT64* protectedMemory = static_cast<UINT64*>(Irp->AssociatedIrp.SystemBuffer);

			if (!VALID_KERNELMODE_MEMORY(*protectedMemory)) {
				status = STATUS_INVALID_PARAMETER;
				break;
			}

			status = static_cast<NTSTATUS>(KeIpiGenericCall(reinterpret_cast<PKIPI_BROADCAST_WORKER>(VmxHelper::UnhookPageByVmcall),
				*protectedMemory));

			if (!NT_SUCCESS(status))
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to unhook page 0x%llx (0x%08X)", *protectedMemory, status);
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