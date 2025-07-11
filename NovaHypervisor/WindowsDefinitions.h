#pragma once

#include "pch.h"

// Globals
inline ULONG WindowsBuildNumber = 0;
inline PVOID AllocatePool2 = NULL;

// Constants
constexpr ULONG WIN_1507 = 10240;
constexpr ULONG WIN_1511 = 10586;
constexpr ULONG WIN_1607 = 14393;
constexpr ULONG WIN_1703 = 15063;
constexpr ULONG WIN_1709 = 16299;
constexpr ULONG WIN_1803 = 17134;
constexpr ULONG WIN_1809 = 17763;
constexpr ULONG WIN_1903 = 18362;
constexpr ULONG WIN_1909 = 18363;
constexpr ULONG WIN_2004 = 19041;
constexpr ULONG WIN_20H2 = 19042;
constexpr ULONG WIN_21H1 = 19043;
constexpr ULONG WIN_21H2 = 19044;
constexpr ULONG WIN_22H2 = 19045;
constexpr ULONG WIN_1121H2 = 22000;
constexpr ULONG WIN_1122H2 = 22621;
constexpr ULONG WIN_1123H2 = 22631;
constexpr ULONG WIN_1124H2 = 26100;

// Structs
typedef struct _KPROCESS
{
	DISPATCHER_HEADER Header;
	LIST_ENTRY ProfileListHead;
	ULONG_PTR DirectoryTableBase;
	UCHAR Data[1];
} KPROCESS, * PKPROCESS;

typedef struct _KERNEL_BASE_INFO {
	ULONG64 KernelBaseAddress;
	ULONG KernelSize;
} KERNEL_BASE_INFO, * PKERNEL_BASE_INFO;

typedef struct _KLDR_DATA_TABLE_ENTRY {
	LIST_ENTRY InLoadOrderLinks;
	VOID* ExceptionTable;
	ULONG ExceptionTableSize;
	VOID* GpValue;
	PVOID* NonPagedDebugInfo;
	VOID* DllBase;
	VOID* EntryPoint;
	ULONG SizeOfImage;
	UNICODE_STRING FullDllName;
	UNICODE_STRING BaseDllName;
	ULONG Flags;
	USHORT LoadCount;
	union
	{
		USHORT SignatureLevel : 4;
		USHORT SignatureType : 3;
		USHORT Frozen : 2;
		USHORT HotPatch : 1;
		USHORT Unused : 6;
		USHORT EntireField;
	} u1;
	VOID* SectionPointer;
	ULONG CheckSum;
	ULONG CoverageSectionSize;
	VOID* CoverageSection;
	VOID* LoadedImports;
	union
	{
		VOID* Spare;
		PVOID* NtDataTableEntry;
	};
	ULONG SizeOfImageNotRounded;
	ULONG TimeDateStamp;
} KLDR_DATA_TABLE_ENTRY, * PKLDR_DATA_TABLE_ENTRY;

#pragma warning (disable: 4201)
struct _EX_PUSH_LOCK
{
	union
	{
		struct
		{
			ULONGLONG Locked : 1;
			ULONGLONG Waiting : 1;
			ULONGLONG Waking : 1;
			ULONGLONG MultipleShared : 1;
			ULONGLONG Shared : 60;
		};
		ULONGLONG Value;
		VOID* Ptr;
	};
};

typedef struct _FULL_OBJECT_TYPE {
	LIST_ENTRY TypeList;
	UNICODE_STRING Name;
	VOID* DefaultObject;
	UCHAR Index;
	ULONG TotalNumberOfObjects;
	ULONG TotalNumberOfHandles;
	ULONG HighWaterNumberOfObjects;
	ULONG HighWaterNumberOfHandles;
	UCHAR TypeInfo[0x78];
	_EX_PUSH_LOCK TypeLock;
	ULONG Key;
	LIST_ENTRY CallbackList;
} FULL_OBJECT_TYPE, * PFULL_OBJECT_TYPE;

// Function prototypes
typedef PVOID(NTAPI* tExAllocatePool2)(
	POOL_FLAGS Flags,
	SIZE_T     NumberOfBytes,
	ULONG      Tag
	);

// Externs
extern "C" PKLDR_DATA_TABLE_ENTRY PsLoadedModuleList;
extern "C" PERESOURCE PsLoadedModuleResource;

extern "C" {
	_IRQL_requires_max_(APC_LEVEL)
		_IRQL_requires_min_(PASSIVE_LEVEL)
		_IRQL_requires_same_
		VOID
		NTAPI
		KeGenericCallDpc(
			_In_ PKDEFERRED_ROUTINE Routine,
			_In_opt_ PVOID Context
		);

	_IRQL_requires_(DISPATCH_LEVEL)
		_IRQL_requires_same_
		VOID
		NTAPI
		KeSignalCallDpcDone(
			_In_ PVOID SystemArgument1
		);

	_IRQL_requires_(DISPATCH_LEVEL)
		_IRQL_requires_same_
		ULONG
		NTAPI
		KeSignalCallDpcSynchronize(
			_In_ PVOID SystemArgument2
		);
}
