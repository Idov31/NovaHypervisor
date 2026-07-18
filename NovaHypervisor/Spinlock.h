#pragma once
#include "pch.h"

constexpr UINT32 MAX_WAIT = 65536;

class Spinlock {
public:
	_IRQL_requires_max_(HIGH_LEVEL) Spinlock();
	_IRQL_requires_max_(HIGH_LEVEL) ~Spinlock();
	_IRQL_requires_max_(HIGH_LEVEL) void Lock();
	_IRQL_requires_max_(HIGH_LEVEL) void Unlock();
private:
	_IRQL_requires_max_(HIGH_LEVEL) bool DoLock();
	volatile SHORT lock;
};
