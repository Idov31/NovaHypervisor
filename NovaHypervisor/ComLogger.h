#pragma once
#include "pch.h"

namespace ComLogger {

enum class Level : UCHAR {
	Debug,
	Info,
	Error
};

enum class ComPorts : USHORT {
	Com1 = 0x3F8,
	Com2 = 0x2F8
};

constexpr USHORT DefaultPort = static_cast<USHORT>(ComPorts::Com2);

void Initialize(_In_ USHORT port = DefaultPort) noexcept;
void Write(_In_ Level level, _In_z_ const char* format, ...) noexcept;
void VWrite(_In_ Level level, _In_z_ const char* format, _In_ va_list args) noexcept;

}

#define TRACE_FLAG_DEBUG ::ComLogger::Level::Debug
#define TRACE_FLAG_INFO ::ComLogger::Level::Info
#define TRACE_FLAG_ERROR ::ComLogger::Level::Error

#define NovaHypervisorLog(Level, Message, ...) \
	::ComLogger::Write((Level), (Message), ##__VA_ARGS__)
