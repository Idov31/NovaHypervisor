#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "GlobalVariables.h"
#include "Vmx.h"
#include "Ept.h"
#include "VmxHelper.h"
#include "WppDefinitions.h"
#include "VmcallHandler.tmh"

NTSTATUS VmcallHandler(_In_ UINT64 vmcallNumber, _In_opt_ UINT64 optionalParam1, _In_opt_ UINT64 optionalParam2, _In_opt_ UINT64 optionalParam3);

NTSTATUS HypercallHandler(_In_ Ept* eptInstance, _Inout_ PGUEST_REGS registers);