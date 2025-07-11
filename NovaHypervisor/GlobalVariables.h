#pragma once
#include "pch.h"
#include "HypervisorDefinitions.h"
#include "Ept.h"
#include "VmState.h"
#include "PoolManager.h"

inline VmState* GuestState;
inline PoolManager* poolManager;
inline KERNEL_BASE_INFO KernelBaseInfo;