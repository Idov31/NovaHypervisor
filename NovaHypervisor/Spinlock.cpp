#include "pch.h"
#include "Spinlock.h"

/*
* Description:
* Spinlock initializes the lock in the unlocked state.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
Spinlock::Spinlock() {
	Unlock();
}

/*
* Description:
* ~Spinlock leaves the lock in the unlocked state before destruction.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
Spinlock::~Spinlock() {
	Unlock();
}

/*
* Description:
* DoLock attempts to acquire the lock with one atomic exchange.
*
* Parameters:
* There are no parameters.
*
* Returns:
* @acquired [bool] -- True if the lock was acquired, otherwise false.
*/
_IRQL_requires_max_(HIGH_LEVEL)
bool Spinlock::DoLock() {
	return !InterlockedExchange16(&lock, 1) && lock != 0;
}

/*
* Description:
* Lock acquires the lock using bounded exponential backoff.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void Spinlock::Lock() {
	UINT32 wait = 1;

	while (!DoLock()) {
		for (UINT32 i = 0; i < wait; i++)
			_mm_pause();
		wait = wait * 2 > MAX_WAIT ? MAX_WAIT : wait * 2;
	}
}

/*
* Description:
* Unlock releases the lock.
*
* Parameters:
* There are no parameters.
*
* Returns:
* There is no return value.
*/
_IRQL_requires_max_(HIGH_LEVEL)
void Spinlock::Unlock() {
	lock = 0;
}
