#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "InlineAsm.h"
#include "WindowsDefinitions.h"
#include "VmState.h"
#include "ComLogger.h"

constexpr wchar_t KERNEL_NAME[] = L"ntoskrnl.exe";
constexpr size_t KERNEL_NAME_LEN = 12;

namespace VmxHelper {
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS VmxInstructionStatusToNtStatus(_In_ UCHAR instructionStatus);
	_IRQL_requires_max_(HIGH_LEVEL)
	void EnableVmxOperation();
	_IRQL_requires_max_(HIGH_LEVEL)
	void DisableVmxOperation();
	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsVmxSupported();
	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsXstateSaveAreaSupported();
	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsCurrentHypervisorHyperV();
	_IRQL_requires_max_(HIGH_LEVEL)
	bool ClearVmcsState(_Inout_ VmState* state);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool LoadVmcs(_Inout_ VmState* state);
	_IRQL_requires_max_(HIGH_LEVEL)
	void ResumeToNextInstruction();
	_IRQL_requires_max_(HIGH_LEVEL)
	bool GetSegmentDescriptor(_Inout_ PSEGMENT_SELECTOR segmentSelector, _In_ USHORT selector, _In_ PVOID gdtBase);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool FillGuestSelectorData(_In_ PVOID gdtBase, _In_ ULONG segmentRegister, _In_ USHORT selector);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool WriteVmcsField(_In_ SIZE_T field, _In_ SIZE_T value);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool TryWriteOptionalVmcsField(_In_ SIZE_T field, _In_ SIZE_T value, _In_ const char* fieldName);
	_IRQL_requires_max_(HIGH_LEVEL)
	ULONG AdjustControls(_In_ ULONG ctl, _In_ ULONG msr);
	_IRQL_requires_max_(HIGH_LEVEL)
	void InitializeVpidSupport();
	_IRQL_requires_max_(HIGH_LEVEL)
	UINT16 GetVpidTagForProcessor(_In_ ULONG processorIndex);
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS InvalidateVpid(_In_opt_ UINT64 vpid = 0, _In_opt_ UINT64 address = 0);
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS InvalidateEpt(_In_opt_ UINT64 context = 0);
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS InvalidateEptByVmcall(_In_opt_ UINT64 context = 0);
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS HookPageByVmcall(_In_opt_ UINT64 context = 0);
	_IRQL_requires_max_(HIGH_LEVEL)
	NTSTATUS UnhookPageByVmcall(_In_opt_ UINT64 context = 0);
	_IRQL_requires_max_(HIGH_LEVEL)
	void RestoreRegisters();
	_IRQL_requires_(PASSIVE_LEVEL)
	NTSTATUS FindKernelBaseAddress();
	_IRQL_requires_max_(HIGH_LEVEL)
	void SetMonitorTrapFlag(_In_ bool set);
};
