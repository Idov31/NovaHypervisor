#include "pch.h"
#include "Utils.h"

/*
 * Description:
 * GetAddress parses a decimal or hexadecimal address string.
 *
 * Parameters:
 * @address [_In_ std::string] -- The address string to parse.
 *
 * Returns:
 * @result [UINT64] -- The parsed address.
 */
UINT64 GetAddress(_In_ std::string address) {
	UINT64 result = atoi(address.c_str());

	if (result != 0)
		return result;

	if (!address.starts_with("0x") && !address.starts_with("0X"))
		throw std::runtime_error("Invalid address format");

	std::stringstream addressStream;
	addressStream << std::hex << address;
	addressStream >> result;
	return result;
}