#pragma once
#include "pch.h"

constexpr const wchar_t* DRIVER_NAME = LR"(\\.\NovaHypervisor)";
constexpr ULONG IOCTL_PROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS);
constexpr ULONG IOCTL_UNPROTECT_ADDRESS_RANGE = CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS);

typedef struct {
	UINT8 Permissions;
	UINT64 Address;
} ProtectedMemory;

enum EptPagePermissions {
	EPT_PAGE_READ = 1,
	EPT_PAGE_WRITE = 2,
	EPT_PAGE_EXECUTE = 4,
	EPT_ALL_PERMISSIONS = EPT_PAGE_READ | EPT_PAGE_WRITE | EPT_PAGE_EXECUTE,
	EPT_IS_EXECUTION_HOOK = 8,
	EPT_MAX_PAGE_PERMISSIONS = EPT_IS_EXECUTION_HOOK | EPT_PAGE_WRITE | EPT_PAGE_READ
};

class NovaHypervisor
{
private:
	HANDLE hNova;

	static constexpr auto ValidHandle = [](HANDLE h) -> bool {
		return h && h != INVALID_HANDLE_VALUE;
	};
	bool IsVmxSupported();

public:
	NovaHypervisor();
	~NovaHypervisor();
	bool ProtectAddressRange(_In_ ProtectedMemory protectedMemory) const;
	bool UnprotectAddressRange(_In_ UINT64 address) const;
	UINT8 TranslatePermissions(_In_ const std::string& permissions) const;
};
