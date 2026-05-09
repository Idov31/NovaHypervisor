#pragma once
#include "pch.h"

class ComLogger {
public:
	enum class Level : UCHAR {
		Debug,
		Info,
		Error
	};

	enum class Port : USHORT {
		Com1 = 0x3F8,
		Com2 = 0x2F8
	};

	static constexpr USHORT DefaultPort = static_cast<USHORT>(Port::Com2);

	ComLogger() noexcept = default;

	_IRQL_requires_max_(HIGH_LEVEL)
	void Initialize(_In_ USHORT port = DefaultPort) noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	void Write(_In_ Level level, _In_z_ const char* format, ...) noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	void VWrite(_In_ Level level, _In_z_ const char* format, _In_ va_list args) noexcept;

private:
	static constexpr SIZE_T MessageBufferSize = 512;
	static constexpr ULONG TransmitReadyRetries = 100000;
	static constexpr ULONG WriteLockRetries = 100000;
	static constexpr UCHAR LineStatusTransmitEmpty = 0x20;
	static constexpr char HexDigits[] = "0123456789abcdef";

	static constexpr USHORT InterruptEnableRegister = 1;
	static constexpr USHORT FifoControlRegister = 2;
	static constexpr USHORT LineControlRegister = 3;
	static constexpr USHORT ModemControlRegister = 4;
	static constexpr USHORT LineStatusRegister = 5;
	static constexpr UCHAR DivisorLatchAccessBit = 0x80;

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteRegister(_In_ USHORT offset, _In_ UCHAR value) const noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	UCHAR ReadRegister(_In_ USHORT offset) const noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	bool WaitUntilTransmitReady() const noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteChar(_In_ char value) const noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	void WriteString(_In_reads_or_z_(maximumLength) const char* value, _In_ SIZE_T maximumLength) const noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	bool TryAcquireWriteLock() noexcept;

	_IRQL_requires_max_(HIGH_LEVEL)
	void ReleaseWriteLock() noexcept;

	void AppendChar(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
		_Inout_ SIZE_T& offset, _In_ char value) const noexcept;

	void AppendString(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
		_Inout_ SIZE_T& offset, _In_reads_or_z_(maximumLength) const char* value,
		_In_ SIZE_T maximumLength) const noexcept;

	void AppendUnsigned(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
		_Inout_ SIZE_T& offset, _In_ UINT64 value, _In_ ULONG radix,
		_In_ bool uppercase) const noexcept;

	void AppendSigned(_Inout_updates_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
		_Inout_ SIZE_T& offset, _In_ INT64 value) const noexcept;

	void FormatMessage(_Out_writes_z_(bufferSize) char* buffer, _In_ SIZE_T bufferSize,
		_In_z_ const char* format, _In_ va_list args) const noexcept;

	const char* PrefixForLevel(_In_ Level level) const noexcept;

	volatile USHORT port_ = DefaultPort;
	volatile LONG writeLock_ = 0;
};

extern ComLogger NovaLogger;

#define TRACE_FLAG_DEBUG ComLogger::Level::Debug
#define TRACE_FLAG_INFO ComLogger::Level::Info
#define TRACE_FLAG_ERROR ComLogger::Level::Error

#define NovaHypervisorLog(Level, Message, ...) \
	::NovaLogger.Write((Level), (Message), ##__VA_ARGS__)
