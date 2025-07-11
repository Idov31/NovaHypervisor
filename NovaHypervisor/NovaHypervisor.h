#pragma once
#include "pch.h"
#include "Vmx.h"
#include "DeviceControl.h"
#include "Ept.h"
#include "VmxHelper.h"
#include "WppDefinitions.h"
#include "NovaHypervisor.tmh"

constexpr wchar_t DEVICE_NAME[] = L"\\Device\\NovaHypervisor";
constexpr wchar_t SYMBOLIC_LINK[] = L"\\??\\NovaHypervisor";

NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath);
DRIVER_UNLOAD NovaUnload;
DRIVER_DISPATCH NovaCreateClose;
