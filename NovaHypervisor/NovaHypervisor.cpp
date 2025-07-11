#include "pch.h"
#include "NovaHypervisor.h"

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);
	UNICODE_STRING deviceName = { 0 };
	UNICODE_STRING symLinkName = { 0 };
	PDEVICE_OBJECT deviceObject = nullptr;
	NTSTATUS status = STATUS_SUCCESS;
	KernelBaseInfo = { 0 };

	// Getting the OS version.
	RTL_OSVERSIONINFOW osVersion = { sizeof(osVersion) };
	NTSTATUS result = RtlGetVersion(&osVersion);

	if (!NT_SUCCESS(result))
		return STATUS_ABANDONED;

	WPP_INIT_TRACING(DriverObject, RegistryPath);
	WindowsBuildNumber = osVersion.dwBuildNumber;

	// Loading ExAllocatePool2 if available.
	UNICODE_STRING routineName = RTL_CONSTANT_STRING(L"ExAllocatePool2");
	AllocatePool2 = MmGetSystemRoutineAddress(&routineName);
	status = VmxHelper::FindKernelBaseAddress();

	if (!NT_SUCCESS(status)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to find kernel base address");
		WPP_CLEANUP(DriverObject);
		return status;
	}

	__try {
		poolManager = new PoolManager();
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = GetExceptionCode();
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Exception occurred while initializing the pool manager: (0x%08X)", status);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	// Initializing the driver.
	RtlInitUnicodeString(&deviceName, DEVICE_NAME);
	RtlInitUnicodeString(&symLinkName, SYMBOLIC_LINK);

	status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &deviceObject);

	if (!NT_SUCCESS(status)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to create device object (0x%08X)", status);
		WPP_CLEANUP(DriverObject);
		return status;
	}
	status = IoCreateSymbolicLink(&symLinkName, &deviceName);

	if (!NT_SUCCESS(status)) {
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to create symbolic link (0x%08X)", status);
		IoDeleteDevice(deviceObject);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	DriverObject->DriverUnload = NovaUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = NovaCreateClose;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = NovaCreateClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = NovaDeviceControl;

	status = VmxInitialize() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

	if (!NT_SUCCESS(status)) {
		TerminateVmx();

		if (poolManager) {
			delete poolManager;
			poolManager = nullptr;
		}
		NovaHypervisorLog(TRACE_FLAG_ERROR, "Failed to initialize the hypervisor (0x%08X)", status);
		IoDeleteSymbolicLink(&symLinkName);
		IoDeleteDevice(deviceObject);
		WPP_CLEANUP(DriverObject);
		return status;
	}

	NovaHypervisorLog(TRACE_FLAG_INFO, "Driver loaded successfully");
	return status;
}

void NovaUnload(_In_ PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING symLinkName = { 0 };
	TerminateVmx();

	if (poolManager) {
		delete poolManager;
		poolManager = nullptr;
	}
	RtlInitUnicodeString(&symLinkName, SYMBOLIC_LINK);
	IoDeleteSymbolicLink(&symLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);

	NovaHypervisorLog(TRACE_FLAG_INFO, "Driver unloaded");
	WPP_CLEANUP(DriverObject);
}

/*
* Description:
* NovaCreateClose is responsible for creating a success response for given IRP.
*
* Parameters:
* @DeviceObject [PDEVICE_OBJECT] -- Not used.
* @Irp			[PIRP]			 -- The IRP that contains the user data such as SystemBuffer, Irp stack, etc.
*
* Returns:
* @status		[NTSTATUS]		 -- Always will be STATUS_SUCCESS.
*/
NTSTATUS NovaCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	UNREFERENCED_PARAMETER(DeviceObject);
	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return STATUS_SUCCESS;
}