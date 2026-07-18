#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "GlobalVariables.h"
#include "Vmx.h"
#include "Ept.h"
#include "VmxHelper.h"
#include "ComLogger.h"

_IRQL_requires_max_(HIGH_LEVEL)
NTSTATUS VmcallHandler(_In_ UINT64 vmcallNumber, _In_opt_ UINT64 optionalParam1, _In_opt_ UINT64 optionalParam2, _In_opt_ UINT64 optionalParam3);

_IRQL_requires_max_(HIGH_LEVEL)
bool HypercallHandler(_In_ Ept* eptInstance, _Inout_ PGUEST_REGS registers, _In_ UINT64 guestFxState);
