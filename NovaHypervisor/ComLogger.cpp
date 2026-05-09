#include "pch.h"
#include "ComLogger.h"

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

_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::Write(_In_ Level level, _In_z_ const char* format, ...) noexcept {
	va_list args;
	va_start(args, format);
	VWrite(level, format, args);
	va_end(args);
}

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

_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteRegister(_In_ USHORT offset, _In_ UCHAR value) const noexcept {
	__outbyte(static_cast<USHORT>(port_ + offset), value);
}

_IRQL_requires_max_(HIGH_LEVEL)
UCHAR ComLogger::ReadRegister(_In_ USHORT offset) const noexcept {
	return __inbyte(static_cast<USHORT>(port_ + offset));
}

_IRQL_requires_max_(HIGH_LEVEL)
bool ComLogger::WaitUntilTransmitReady() const noexcept {
	for (ULONG attempt = 0; attempt < TransmitReadyRetries; ++attempt) {
		if ((ReadRegister(LineStatusRegister) & LineStatusTransmitEmpty) != 0)
			return true;
	}

	return false;
}

_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteChar(_In_ char value) const noexcept {
	if (WaitUntilTransmitReady())
		WriteRegister(0, static_cast<UCHAR>(value));
}

_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::WriteString(_In_reads_or_z_(maximumLength) const char* value, _In_ SIZE_T maximumLength) const noexcept {
	if (!value)
		return;

	for (SIZE_T index = 0; index < maximumLength && value[index] != '\0'; ++index)
		WriteChar(value[index]);
}

_IRQL_requires_max_(HIGH_LEVEL)
bool ComLogger::TryAcquireWriteLock() noexcept {
	for (ULONG attempt = 0; attempt < WriteLockRetries; ++attempt) {
		if (InterlockedCompareExchange(&writeLock_, 1, 0) == 0)
			return true;

		_mm_pause();
	}

	return false;
}

_IRQL_requires_max_(HIGH_LEVEL)
void ComLogger::ReleaseWriteLock() noexcept {
	InterlockedExchange(&writeLock_, 0);
}

void ComLogger::AppendChar(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_ char value) const noexcept {
	if (!buffer || bufferSize == 0 || offset >= bufferSize - 1)
		return;

	buffer[offset++] = value;
	buffer[offset] = '\0';
}

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

void ComLogger::AppendSigned(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
	_Inout_ SIZE_T& offset, _In_ INT64 value) const noexcept {
	UINT64 magnitude = static_cast<UINT64>(value);

	if (value < 0) {
		AppendChar(buffer, bufferSize, offset, '-');
		magnitude = 0 - magnitude;
	}

	AppendUnsigned(buffer, bufferSize, offset, magnitude, 10, false);
}

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
