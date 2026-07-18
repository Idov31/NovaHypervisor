#pragma once
#include "pch.h"
#include "Vmx.h"
#include "DeviceControl.h"
#include "Ept.h"
#include "VmxHelper.h"
#include "ComLogger.h"

constexpr wchar_t DEVICE_NAME[] = L"\\Device\\NovaHypervisor";
constexpr wchar_t SYMBOLIC_LINK[] = L"\\??\\NovaHypervisor";

DRIVER_INITIALIZE DriverEntry;

DRIVER_UNLOAD NovaUnload;

PDRIVER_DISPATCH NovaCreateClose;
