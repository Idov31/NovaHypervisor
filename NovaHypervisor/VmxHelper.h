#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "InlineAsm.h"
#include "WindowsDefinitions.h"
#include "VmState.h"
#include "WppDefinitions.h"
#include "VmxHelper.tmh"

constexpr wchar_t KERNEL_NAME[] = L"ntoskrnl.exe";
constexpr size_t KERNEL_NAME_LEN = 12;

namespace VmxHelper {
	void EnableVmxOperation();
	void DisableVmxOperation();
	bool IsVmxSupported();
	bool ClearVmcsState(_Inout_ VmState* state);
	bool LoadVmcs(_Inout_ VmState* state);
	void ResumeToNextInstruction();
	bool GetSegmentDescriptor(_Inout_ PSEGMENT_SELECTOR segmentSelector, _In_ USHORT selector, _In_ PVOID gdtBase);
	bool FillGuestSelectorData(_In_ PVOID gdtBase, _In_ ULONG segmentRegister, _In_ USHORT selector);
	ULONG AdjustControls(_In_ ULONG ctl, _In_ ULONG msr);
	void InvalidateVpid(_In_opt_ UINT64 vpid = 0, _In_opt_ UINT64 address = 0);
	void InvalidateEpt(_In_opt_ UINT64 context = 0);
	NTSTATUS InvalidateEptByVmcall(_In_opt_ UINT64 context = 0);
	NTSTATUS HookPageByVmcall(_In_opt_ UINT64 context = 0);
	NTSTATUS UnhookPageByVmcall(_In_opt_ UINT64 context = 0);
	void RestoreRegisters();
	UINT64 FindSystemDirectoryTableBase();
	NTSTATUS FindKernelBaseAddress();
	void SetMonitorTrapFlag(_In_ bool set);
};
