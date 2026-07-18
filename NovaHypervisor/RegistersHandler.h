#pragma once

#include "pch.h"
#include "HypervisorDefinitions.h"
#include "VmxHelper.h"
#include "ComLogger.h"

constexpr int HYPERVISOR_INTERFACE = 'AVON';

namespace RegistersHandler {
	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsHyperVSyntheticMsr(_In_ ULONG64 msr);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool IsValidMsr(_In_ ULONG64 rcx);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool HandleCRAccess(_In_ PGUEST_REGS guestRegisters);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool HandleMSRRead(_Inout_ PGUEST_REGS guestRegisters);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool HandleMSRWrite(_In_ PGUEST_REGS guestRegisters);
	_IRQL_requires_max_(HIGH_LEVEL)
	bool HandleCpuid(_Inout_ PGUEST_REGS guestRegisters);
};
