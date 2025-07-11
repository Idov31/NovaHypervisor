#include "pch.h"
#include "VmcallHandler.h"

/*
* Description:
* VmcallHandler is responsible for handling the vmcalls.
*
* Parameters:
* @vmcallNumber	  [_In_ UINT64]		-- The vmcall number.
* @optionalParam1 [_In_opt_ UINT64] -- The first optional parameter.
* @optionalParam2 [_In_opt_ UINT64] -- The second optional parameter.
* @optionalParam3 [_In_opt_ UINT64] -- The third optional parameter.
*
* Returns:
* @status		  [NTSTATUS]		-- The function returns the status of the operation.
*/
NTSTATUS VmcallHandler(_In_ UINT64 vmcallNumber, _In_opt_ UINT64 optionalParam1, _In_opt_ UINT64 optionalParam2, _In_opt_ UINT64 optionalParam3) {
	NTSTATUS status = STATUS_SUCCESS;
	ULONG currentProcessorIndex = KeGetCurrentProcessorNumber();
	Ept* currentEptInstance = GuestState[currentProcessorIndex].EptInstance;

	switch (vmcallNumber) {
		case VMCALL_TEST: {
			NovaHypervisorLog(TRACE_FLAG_INFO, "VmcallTest called with @Param1 = 0x%llx , @Param2 = 0x%llx , @Param3 = 0x%llx", optionalParam1, optionalParam2, optionalParam3);
			break;
		}
		case VMCALL_VMXOFF: {
			VmxVmxoff();
			break;
		}
		case VMCALL_EXEC_HOOK_PAGE: {
			if (optionalParam1 == 0) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Invalid parameters for VMCALL_EXEC_HOOK_PAGE");
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			status = currentEptInstance->RootModePageHook(reinterpret_cast<PVOID>(optionalParam1), static_cast<UINT8>(optionalParam2)) ?
				STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
			break;
		}
		case VMCALL_INVEPT_SINGLE_CONTEXT: {
			VmxHelper::InvalidateEpt(optionalParam1);
			break;
		}
		case VMCALL_INVEPT_ALL_CONTEXT: {
			VmxHelper::InvalidateEpt();
			break;
		}
		case VMCALL_UNHOOK_SINGLE_PAGE: {
			if (optionalParam1 == 0) {
				NovaHypervisorLog(TRACE_FLAG_ERROR, "Invalid parameters for VMCALL_UNHOOK_SINGLE_PAGE");
				status = STATUS_INVALID_PARAMETER;
				break;
			}
			status = currentEptInstance->PageUnhookVmcall(optionalParam1) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
			break;
		}
		case VMCALL_UNHOOK_ALL_PAGES: {
			status = currentEptInstance->UnhookAllPagesVmcall() ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
			break;
		}
		default: {
			NovaHypervisorLog(TRACE_FLAG_WARNING, "Unsupported vmcall: 0x%llx\n", vmcallNumber);
			status = STATUS_INVALID_PARAMETER;
			break;
		}
	}
	return status;
}
