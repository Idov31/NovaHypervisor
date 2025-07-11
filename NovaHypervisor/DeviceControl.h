#pragma once
#include "pch.h"
#include "GlobalVariables.h"
#include "HypervisorDefinitions.h"
#include "Ept.h"
#include "WppDefinitions.h"
#include "DeviceControl.tmh"

constexpr int IOCTL_PROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
constexpr int IOCTL_UNPROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);

constexpr auto IsValidSize = [](_In_ size_t dataSize, _In_ size_t structSize) -> bool {
	return dataSize != 0 && dataSize % structSize == 0;
};

constexpr auto VALID_KERNELMODE_MEMORY = [](_In_ size_t address) -> bool {
	return address > 0x8000000000000000 && address < 0xFFFFFFFFFFFFFFFF;
};

DRIVER_DISPATCH NovaDeviceControl;