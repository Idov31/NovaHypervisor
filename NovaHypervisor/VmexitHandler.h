#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "GlobalVariables.h"
#include "RegistersHandler.h"
#include "EventInjection.h"
#include "VmcallHandler.h"
#include "WppDefinitions.h"
#include "VmexitHandler.tmh"

constexpr ULONG64 NO_HYPERV_MAGIC = 0x4e4f485950455256;
constexpr ULONG64 VMCALL_MAGIC = 0x564d43414c4c;
constexpr ULONG64 HYPERVISOR_MAGIC = 0x4856544d50;

constexpr auto IsSelfVmcall = [](ULONG64 r10, ULONG64 r11, ULONG64 r12) -> bool {
	return r10 == HYPERVISOR_MAGIC && r11 == VMCALL_MAGIC && r12 == NO_HYPERV_MAGIC;
};

extern "C" {
	void VmResumeInstruction();
	bool VmexitHandler(_Inout_ PGUEST_REGS guestRegisters);
}
