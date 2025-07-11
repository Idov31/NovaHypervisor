#pragma once

#include "pch.h"
#include "AutoLock.hpp"
#include "Spinlock.h"

constexpr SIZE_T MAX_ALLOCATION_REQUESTS = 100;

template <typename Request>
class RequestList {
private:
	typedef struct _RequestItem {
		Request Data;
		bool IsUsed;
	} RequestItem;
	Spinlock lock;
	RequestItem requests[MAX_ALLOCATION_REQUESTS];
	UINT64 itemsCount = 0;

public:
	RequestList() {
		memset(requests, 0, sizeof(requests));
		lock = Spinlock();
	}

	~RequestList() {
		memset(requests, 0, sizeof(requests));
	}

	bool Insert(Request item) {
		AutoLock<Spinlock> autoLock(lock);

		if (itemsCount == MAX_ALLOCATION_REQUESTS)
			return false;

		for (SIZE_T i = 0; i < MAX_ALLOCATION_REQUESTS; i++) {
			if (!requests[i].IsUsed) {
				requests[i].Data = item;
				requests[i].IsUsed = true;
				itemsCount++;
				return true;
			}
		}
		return false;
	}

	Request Pop() {
		AutoLock<Spinlock> autoLock(lock);

		if (itemsCount == 0)
			return Request();

		for (SIZE_T i = 0; i < MAX_ALLOCATION_REQUESTS; i++) {
			if (requests[i].IsUsed) {
				requests[i].IsUsed = false;
				itemsCount--;
				return requests[i].Data;
			}
		}
		return Request();
	}

	UINT64 GetListSize() {
		AutoLock<Spinlock> autoLock(lock);
		return itemsCount;
	}
};