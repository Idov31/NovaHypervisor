#include "pch.h"
#include "NovaHypervisor.h"

NovaHypervisor::NovaHypervisor() {
	hNova = NULL;

	if (!IsVmxSupported())
		throw std::runtime_error("VMX is not supported on this system");

	hNova = CreateFile(DRIVER_NAME, GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);

	if (!ValidHandle(hNova))
		throw std::runtime_error("Failed to open Nova driver");
}

NovaHypervisor::~NovaHypervisor() {
	if (hNova && hNova != INVALID_HANDLE_VALUE)
		CloseHandle(hNova);
}

/*
 * Description:
 * IsVmxSupported checks if VMX is supported on the current system.
 *
 * Parameters:
 * There are no parameters.
 *
 * Returns:
 * @supported [bool] -- True if VMX is supported, false otherwise.
 */
bool NovaHypervisor::IsVmxSupported() {
	std::array<int, 4> cpuInfo{};
	__cpuid(cpuInfo.data(), 1);
	return (cpuInfo[2] & (1 << 5)) != 0;
}

/*
 * Description:
 * ProtectAddressRange protects a range of memory.
 *
 * Parameters:
 * @protectedMemory [_In_ ProtectedMemory] -- The protected memory structure.
 *
 * Returns:
 * @protected	   [bool]				   -- True if the address was protected successfully, false otherwise.
 */
bool NovaHypervisor::ProtectAddressRange(_In_ ProtectedMemory protectedMemory) const {
	if (!ValidHandle(hNova))
		return false;
	DWORD bytesReturned = 0;
	BOOL result = DeviceIoControl(hNova, IOCTL_PROTECT_ADDRESS_RANGE, &protectedMemory, sizeof(protectedMemory),
		nullptr, 0, &bytesReturned, nullptr);
	return result == TRUE;
}

/*
 * Description:
 * UnprotectAddressRange unprotects a range of memory.
 *
 * Parameters:
 * @address	  [_In_ UINT64] -- The address to unprotect.
 *
 * Returns:
 * @protected [bool]		-- True if the address was unprotected successfully, false otherwise.
 */
bool NovaHypervisor::UnprotectAddressRange(_In_ UINT64 address) const {
	if (!ValidHandle(hNova))
		return false;
	DWORD bytesReturned = 0;
	BOOL result = DeviceIoControl(hNova, IOCTL_UNPROTECT_ADDRESS_RANGE, &address, sizeof(address),
		nullptr, 0, &bytesReturned, nullptr);
	return result == TRUE;
}

/*
 * Description:
 * TranslatePermissions translates the permissions string to a UINT8 value.
 *
 * Parameters:
 * @permissions [_In_ const std::string&] -- The permissions string.
 *
 * Returns:
 * @result	    [UINT8]				      -- The translated permissions.
 */
UINT8 NovaHypervisor::TranslatePermissions(_In_ const std::string& permissions) const {
	UINT8 result = 0;

	if (permissions.size() > 3)
		throw std::runtime_error("Invalid permissions string");

	if (permissions.find('r') != std::string::npos)
		result |= EPT_PAGE_READ;
	if (permissions.find('w') != std::string::npos)
		result |= EPT_PAGE_WRITE;
	if (permissions.find('x') != std::string::npos)
		result |= EPT_PAGE_EXECUTE;
	return result;
}