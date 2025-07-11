#pragma once
#include "pch.h"

constexpr UINT32 MAX_WAIT = 65536;

class Spinlock {
public:
	Spinlock();
	~Spinlock();
	void Lock();
	void Unlock();
private:
	bool DoLock();
	volatile SHORT lock;
};