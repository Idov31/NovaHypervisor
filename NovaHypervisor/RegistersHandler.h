#pragma once

#include "pch.h"
#include "HypervisorDefinitions.h"
#include "VmxHelper.h"
#include "WppDefinitions.h"
#include "RegistersHandler.tmh"

constexpr int HYPERVISOR_INTERFACE = 'AVON';

namespace RegistersHandler {
	bool IsValidMsr(_In_ ULONG64 rcx);
	void HandleCRAccess(_In_ PGUEST_REGS guestRegisters);
	void HandleMSRRead(_Inout_ PGUEST_REGS guestRegisters);
	void HandleMSRWrite(_In_ PGUEST_REGS guestRegisters);
	void HandleCpuid(_Inout_ PGUEST_REGS guestRegisters);
};