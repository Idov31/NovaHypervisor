#pragma once
#include "pch.h"

template<typename TLock>
struct AutoLock {
	/*
	* Description:
	* AutoLock acquires the supplied lock for the lifetime of this object.
	*
	* Parameters:
	* @lock [TLock&] -- The lock to acquire.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
	AutoLock(TLock& lock) : _lock(lock) {
		_lock.Lock();
	}

	/*
	* Description:
	* ~AutoLock releases the lock acquired by the constructor.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
	~AutoLock() {
		_lock.Unlock();
	}

private:
	TLock& _lock;
};
