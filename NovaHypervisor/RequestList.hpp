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
	/*
	* Description:
	* RequestList initializes an empty fixed-capacity request list.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
	RequestList() {
		memset(requests, 0, sizeof(requests));
		lock = Spinlock();
	}

	/*
	* Description:
	* ~RequestList clears the request storage before destruction.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* There is no return value.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
	~RequestList() {
		memset(requests, 0, sizeof(requests));
	}

	/*
	* Description:
	* Insert adds a request to the fixed-capacity list.
	*
	* Parameters:
	* @item [Request] -- The request to insert.
	*
	* Returns:
	* @inserted [bool] -- True if the request was inserted, otherwise false.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
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

	/*
	* Description:
	* Pop removes and returns the next queued request.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* @request [Request] -- The next request, or a default value if the list is empty.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
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

	/*
	* Description:
	* GetListSize returns the number of currently queued requests.
	*
	* Parameters:
	* There are no parameters.
	*
	* Returns:
	* @size [UINT64] -- The number of queued requests.
	*/
	_IRQL_requires_max_(HIGH_LEVEL)
	UINT64 GetListSize() {
		AutoLock<Spinlock> autoLock(lock);
		return itemsCount;
	}
};
