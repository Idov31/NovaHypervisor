#include "pch.h"
#include "Spinlock.h"

Spinlock::Spinlock() {
	Unlock();
}

Spinlock::~Spinlock() {
	Unlock();
}

bool Spinlock::DoLock() {
	return !InterlockedExchange16(&lock, 1) && lock != 0;
}

void Spinlock::Lock() {
	UINT32 wait = 1;

	while (!DoLock()) {
		for (UINT32 i = 0; i < wait; i++)
			_mm_pause();
		wait = wait * 2 > MAX_WAIT ? MAX_WAIT : wait * 2;
	}
}

void Spinlock::Unlock() {
	lock = 0;
}