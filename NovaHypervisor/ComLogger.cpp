#include "pch.h"
#include "ComLogger.h"

namespace {

	constexpr SIZE_T MessageBufferSize = 512;
	constexpr ULONG TransmitReadyRetries = 100000;
	constexpr ULONG WriteLockRetries = 100000;
	constexpr UCHAR LineStatusTransmitEmpty = 0x20;

	constexpr USHORT InterruptEnableRegister = 1;
	constexpr USHORT FifoControlRegister = 2;
	constexpr USHORT LineControlRegister = 3;
	constexpr USHORT ModemControlRegister = 4;
	constexpr USHORT LineStatusRegister = 5;
	constexpr UCHAR DivisorLatchAccessBit = 0x80;

	volatile USHORT g_Port = ComLogger::DefaultPort;
	volatile LONG g_WriteLock = 0;

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteRegister(_In_ USHORT offset, _In_ UCHAR value) noexcept {
		__outbyte(static_cast<USHORT>(g_Port + offset), value);
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	UCHAR ReadRegister(_In_ USHORT offset) noexcept {
		return __inbyte(static_cast<USHORT>(g_Port + offset));
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	bool WaitUntilTransmitReady() noexcept {
		for (ULONG attempt = 0; attempt < TransmitReadyRetries; ++attempt) {
			if ((ReadRegister(LineStatusRegister) & LineStatusTransmitEmpty) != 0)
				return true;
		}
		return false;
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteChar(_In_ char value) noexcept {
		if (WaitUntilTransmitReady())
			WriteRegister(0, static_cast<UCHAR>(value));
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteString(_In_reads_or_z_(maximumLength) const char* value, _In_ SIZE_T maximumLength) noexcept {
		if (!value)
			return;

		for (SIZE_T index = 0; index < maximumLength && value[index] != '\0'; ++index)
			WriteChar(value[index]);
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	bool TryAcquireWriteLock() noexcept {
		for (ULONG attempt = 0; attempt < WriteLockRetries; ++attempt) {
			if (InterlockedCompareExchange(&g_WriteLock, 1, 0) == 0)
				return true;

			_mm_pause();
		}

		return false;
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	void ReleaseWriteLock() noexcept {
		InterlockedExchange(&g_WriteLock, 0);
	}

	_IRQL_requires_max_(HIGH_LEVEL)
	const char* PrefixForLevel(_In_ ComLogger::Level level) noexcept {
		switch (level) {
		case ComLogger::Level::Debug:
			return "[NovaHypervisor][DEBUG] ";
		case ComLogger::Level::Info:
			return "[NovaHypervisor][INFO] ";
		case ComLogger::Level::Error:
			return "[NovaHypervisor][ERROR] ";
		default:
			return "[NovaHypervisor][ERROR] ";
		}
	}

}

namespace ComLogger {

	void Initialize(_In_ USHORT port) noexcept {
		g_Port = port;

		WriteRegister(InterruptEnableRegister, 0x00);
		WriteRegister(LineControlRegister, DivisorLatchAccessBit);
		WriteRegister(0, 0x01);
		WriteRegister(InterruptEnableRegister, 0x00);
		WriteRegister(LineControlRegister, 0x03);
		WriteRegister(FifoControlRegister, 0xC7);
		WriteRegister(ModemControlRegister, 0x0B);
	}

	void VWrite(_In_ Level level, _In_z_ const char* format, _In_ va_list args) noexcept {
		char message[MessageBufferSize] = { 0 };

		if (format) {
			const NTSTATUS status = RtlStringCbVPrintfA(message, sizeof(message), format, args);

			if (!NT_SUCCESS(status))
				RtlStringCbCopyA(message, sizeof(message), "log message formatting failed");
		}
		else {
			RtlStringCbCopyA(message, sizeof(message), "null log format string");
		}

		if (!TryAcquireWriteLock())
			return;

		WriteString(PrefixForLevel(level), MessageBufferSize);
		WriteString(message, sizeof(message));
		WriteString("\r\n", 3);
		ReleaseWriteLock();
	}

	void Write(_In_ Level level, _In_z_ const char* format, ...) noexcept {
		va_list args;
		va_start(args, format);
		VWrite(level, format, args);
		va_end(args);
	}
}
