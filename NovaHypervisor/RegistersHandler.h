#pragma once

#include "pch.h"
#include "HypervisorDefinitions.h"
#include "VmxHelper.h"
#include "ComLogger.h"

constexpr int HYPERVISOR_INTERFACE = 'AVON';

namespace RegistersHandler {
	bool IsHyperVSyntheticMsr(_In_ ULONG64 msr);
	bool IsValidMsr(_In_ ULONG64 rcx);
	bool HandleCRAccess(_In_ PGUEST_REGS guestRegisters);
	bool HandleMSRRead(_Inout_ PGUEST_REGS guestRegisters);
	bool HandleMSRWrite(_In_ PGUEST_REGS guestRegisters);
	bool HandleCpuid(_Inout_ PGUEST_REGS guestRegisters);
};
