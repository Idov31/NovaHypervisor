#include "pch.h"
#include "ComLogger.h"

/*
* Description:
* ComLogger initializes a logger that targets the default serial port.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
ComLogger::ComLogger() noexcept = default;

/*
* Description:
* Initialize configures the selected UART for polled serial output.
*
* Parameters:
* @port [_In_ USHORT] -- The UART base port.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::Initialize(_In_ USHORT port) noexcept {
	port_ = port;

	WriteRegister(InterruptEnableRegister, 0x00);
	WriteRegister(LineControlRegister, DivisorLatchAccessBit);
	WriteRegister(0, 0x01);
	WriteRegister(InterruptEnableRegister, 0x00);
	WriteRegister(LineControlRegister, 0x03);
	WriteRegister(FifoControlRegister, 0xC7);
	WriteRegister(ModemControlRegister, 0x0B);
}

/*
* Description:
* Write formats and writes a serial log message.
*
* Parameters:
* @level  [_In_ Level]         -- The message severity.
* @format [_In_z_ const char*] -- The message format string.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::Write(_In_ Level level, _In_z_ const char* format, ...) noexcept {
	va_list args;
	va_start(args, format);
	VWrite(level, format, args);
	va_end(args);
}

/*
* Description:
* VWrite formats and writes a serial log message from a va_list.
*
* Parameters:
* @level  [_In_ Level]         -- The message severity.
* @format [_In_z_ const char*] -- The message format string.
* @args   [_In_ va_list]       -- The format arguments.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::VWrite(_In_ Level level, _In_z_ const char* format, _In_ va_list args) noexcept {
	char message[MessageBufferSize] = { 0 };

	FormatMessage(message, sizeof(message), format, args);

	if (!TryAcquireWriteLock())
		return;

	WriteString(PrefixForLevel(level), MessageBufferSize);
	WriteString(message, sizeof(message));
	WriteString("\r\n", 3);
	ReleaseWriteLock();
}

/*
* Description:
* WriteRegister writes one value to a UART register.
*
* Parameters:
* @offset [_In_ USHORT] -- The register offset from the UART base port.
* @value  [_In_ UCHAR]  -- The value to write.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteRegister(_In_ USHORT offset, _In_ UCHAR value) const noexcept {
	__outbyte(static_cast<USHORT>(port_ + offset), value);
}

/*
* Description:
* ReadRegister reads one value from a UART register.
*
* Parameters:
* @offset [_In_ USHORT] -- The register offset from the UART base port.
*
* Returns:
* @value [UCHAR] -- The register value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
UCHAR ComLogger::ReadRegister(_In_ USHORT offset) const noexcept {
	return __inbyte(static_cast<USHORT>(port_ + offset));
}

/*
* Description:
* WaitUntilTransmitReady polls the UART until its transmitter is ready or the retry limit is reached.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @ready [bool] -- True if the transmitter became ready, otherwise false.
*/
_IRQL_requires_max_(HIGH_LEVEL)
bool ComLogger::WaitUntilTransmitReady() const noexcept {
	for (ULONG attempt = 0; attempt < TransmitReadyRetries; ++attempt) {
		if ((ReadRegister(LineStatusRegister) & LineStatusTransmitEmpty) != 0)
			return true;
	}

	return false;
}

/*
* Description:
* WriteChar writes one character to the UART when the transmitter is ready.
*
* Parameters:
* @value [_In_ char] -- The character to write.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteChar(_In_ char value) const noexcept {
	if (WaitUntilTransmitReady())
		WriteRegister(0, static_cast<UCHAR>(value));
}

/*
* Description:
* WriteString writes a bounded null-terminated string to the UART.
*
* Parameters:
* @value         [_In_reads_or_z_(maximumLength) const char*] -- The string to write.
* @maximumLength [_In_ SIZE_T]                                -- The maximum number of characters to inspect.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteString(_In_reads_or_z_(maximumLength) const char* value, _In_ SIZE_T maximumLength) const noexcept {
	if (!value)
		return;

	for (SIZE_T index = 0; index < maximumLength && value[index] != '\0'; ++index)
		WriteChar(value[index]);
}

/*
* Description:
* TryAcquireWriteLock attempts to acquire the logger's bounded spin lock.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @acquired [bool] -- True if the lock was acquired, otherwise false.
*/
_IRQL_requires_max_(HIGH_LEVEL)
bool ComLogger::TryAcquireWriteLock() noexcept {
	for (ULONG attempt = 0; attempt < WriteLockRetries; ++attempt) {
		if (InterlockedCompareExchange(&writeLock_, 1, 0) == 0)
			return true;

		_mm_pause();
	}

	return false;
}

/*
* Description:
* ReleaseWriteLock releases the logger's write lock.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::ReleaseWriteLock() noexcept {
	InterlockedExchange(&writeLock_, 0);
}

/*
* Description:
* AppendChar appends one character while preserving null termination.
*
* Parameters:
* @buffer     [_Inout_updates_(bufferSize) char*] -- The destination buffer.
* @bufferSize [_In_ SIZE_T]                       -- The destination buffer size.
* @offset     [_Inout_ SIZE_T&]                   -- The current write offset.
* @value      [_In_ char]                         -- The character to append.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::AppendChar(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_ char value) const noexcept {
	if (!buffer || bufferSize == 0 || offset >= bufferSize - 1)
		return;

	buffer[offset++] = value;
	buffer[offset] = '\0';
}

/*
* Description:
* AppendString appends a bounded string while preserving null termination.
*
* Parameters:
* @buffer        [_Inout_updates_(bufferSize) char*]          -- The destination buffer.
* @bufferSize    [_In_ SIZE_T]                                -- The destination buffer size.
* @offset        [_Inout_ SIZE_T&]                            -- The current write offset.
* @value         [_In_reads_or_z_(maximumLength) const char*] -- The source string.
* @maximumLength [_In_ SIZE_T]                                -- The maximum source length.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::AppendString(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_reads_or_z_(maximumLength) const char* value,
	_In_ SIZE_T maximumLength) const noexcept {
	if (!value) {
		value = "(null)";
		maximumLength = 6;
	}

	for (SIZE_T index = 0; index < maximumLength && value[index] != '\0'; ++index)
		AppendChar(buffer, bufferSize, offset, value[index]);
}

/*
* Description:
* AppendUnsigned formats and appends an unsigned integer.
*
* Parameters:
* @buffer     [_Inout_updates_(bufferSize) char*] -- The destination buffer.
* @bufferSize [_In_ SIZE_T]                       -- The destination buffer size.
* @offset     [_Inout_ SIZE_T&]                   -- The current write offset.
* @value      [_In_ UINT64]                       -- The value to format.
* @radix      [_In_ ULONG]                        -- The numeric radix.
* @uppercase  [_In_ bool]                         -- Whether hexadecimal digits use uppercase.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::AppendUnsigned(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_ UINT64 value, _In_ ULONG radix,
	_In_ bool uppercase) const noexcept {
	char digits[32] = { 0 };
	SIZE_T digitCount = 0;

	if (radix < 2 || radix > 16)
		return;

	do {
		char digit = HexDigits[value % radix];
		if (uppercase && digit >= 'a' && digit <= 'f')
			digit = static_cast<char>(digit - ('a' - 'A'));
		digits[digitCount++] = digit;
		value /= radix;
	} while (value != 0 && digitCount < RTL_NUMBER_OF(digits));

	while (digitCount > 0) {
		--digitCount;

		if (digitCount > RTL_NUMBER_OF(digits))
			break;
		AppendChar(buffer, bufferSize, offset, digits[digitCount]);
	}
}

/*
* Description:
* AppendSigned formats and appends a signed integer.
*
* Parameters:
* @buffer     [_Inout_updates_(bufferSize) char*] -- The destination buffer.
* @bufferSize [_In_ SIZE_T]                       -- The destination buffer size.
* @offset     [_Inout_ SIZE_T&]                   -- The current write offset.
* @value      [_In_ INT64]                        -- The value to format.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::AppendSigned(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_ INT64 value) const noexcept {
	UINT64 magnitude = static_cast<UINT64>(value);

	if (value < 0) {
		AppendChar(buffer, bufferSize, offset, '-');
		magnitude = 0 - magnitude;
	}

	AppendUnsigned(buffer, bufferSize, offset, magnitude, 10, false);
}

/*
* Description:
* FormatMessage formats a bounded serial log message.
*
* Parameters:
* @buffer     [_Out_writes_z_(bufferSize) char*] -- The destination buffer.
* @bufferSize [_In_ SIZE_T]                      -- The destination buffer size.
* @format     [_In_z_ const char*]                -- The message format string.
* @args       [_In_ va_list]                      -- The format arguments.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::FormatMessage(_Out_writes_z_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_In_z_ const char* format, _In_ va_list args) const noexcept {
	SIZE_T offset = 0;

	if (!buffer || bufferSize == 0)
		return;

	buffer[0] = '\0';

	if (!format) {
		AppendString(buffer, bufferSize, offset, "null log format string", MessageBufferSize);
		return;
	}

	for (SIZE_T index = 0; format[index] != '\0'; ++index) {
		if (format[index] != '%') {
			AppendChar(buffer, bufferSize, offset, format[index]);
			continue;
		}

		++index;

		if (format[index] == '%') {
			AppendChar(buffer, bufferSize, offset, '%');
			continue;
		}

		while (format[index] >= '0' && format[index] <= '9')
			++index;

		bool longLongArgument = false;
		bool longArgument = false;
		if (format[index] == 'l') {
			longArgument = true;
			++index;
			if (format[index] == 'l') {
				longLongArgument = true;
				++index;
			}
		}

		switch (format[index]) {
		case 'd':
		case 'i':
			AppendSigned(buffer, bufferSize, offset,
				longLongArgument ? va_arg(args, INT64) :
				longArgument ? va_arg(args, long) : va_arg(args, int));
			break;
		case 'u':
			AppendUnsigned(buffer, bufferSize, offset,
				longLongArgument ? va_arg(args, UINT64) :
				longArgument ? va_arg(args, unsigned long) : va_arg(args, unsigned int), 10, false);
			break;
		case 'x':
			AppendUnsigned(buffer, bufferSize, offset,
				longLongArgument ? va_arg(args, UINT64) :
				longArgument ? va_arg(args, unsigned long) : va_arg(args, unsigned int), 16, false);
			break;
		case 'X':
			AppendUnsigned(buffer, bufferSize, offset,
				longLongArgument ? va_arg(args, UINT64) :
				longArgument ? va_arg(args, unsigned long) : va_arg(args, unsigned int), 16, true);
			break;
		case 'p':
			AppendString(buffer, bufferSize, offset, "0x", 2);
			AppendUnsigned(buffer, bufferSize, offset, reinterpret_cast<UINT64>(va_arg(args, PVOID)), 16, false);
			break;
		case 's':
			AppendString(buffer, bufferSize, offset, va_arg(args, const char*), MessageBufferSize);
			break;
		default:
			AppendChar(buffer, bufferSize, offset, '%');
			if (longArgument) {
				AppendChar(buffer, bufferSize, offset, 'l');
				if (longLongArgument)
					AppendChar(buffer, bufferSize, offset, 'l');
			}
			AppendChar(buffer, bufferSize, offset, format[index]);
			break;
		}
	}
}

/*
* Description:
* PrefixForLevel returns the static prefix for a log severity.
*
* Parameters:
* @level [_In_ Level] -- The log severity.
*
* Returns:
* @prefix [const char*] -- The static severity prefix.
*/
_IRQL_requires_max_(HIGH_LEVEL)
const char* ComLogger::PrefixForLevel(_In_ Level level) const noexcept {
	switch (level) {
	case Level::Debug:
		return "[NovaHypervisor][DEBUG] ";
	case Level::Info:
		return "[NovaHypervisor][INFO] ";
	case Level::Error:
		return "[NovaHypervisor][ERROR] ";
	default:
		return "[NovaHypervisor][ERROR] ";
	}
}
