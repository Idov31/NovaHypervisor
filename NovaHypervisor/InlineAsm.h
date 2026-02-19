#pragma once
#include "pch.h"

extern "C" {
	void inline AsmRestoreToVmxOffState();
	NTSTATUS inline AsmVmxVmcall(unsigned long long VmcallNumber, unsigned long long OptionalParam1, unsigned long long OptionalParam2, unsigned long long OptionalParam3);
	UINT64 inline AsmHypervVmcall(unsigned long long GuestRegisters);
	void AsmVmxSaveState();
	void AsmVmxRestoreState();

	void AsmVmexitHandler();
	void inline  AsmSaveVmxOffState();

	unsigned char inline AsmInvept(unsigned long Type, void* Descriptors);
	unsigned char inline AsmInvvpid(unsigned long Type, void* Descriptors);

	unsigned short AsmGetCs();
	unsigned short AsmGetDs();
	unsigned short AsmGetEs();
	unsigned short AsmGetSs();
	unsigned short AsmGetFs();
	unsigned short AsmGetGs();
	unsigned short AsmGetLdtr();
	unsigned short AsmGetTr();
	unsigned short AsmGetRflags();

	unsigned long long inline AsmGetGdtBase();
	unsigned short AsmGetGdtLimit();

	unsigned long long inline AsmGetIdtBase();
	unsigned short AsmGetIdtLimit();
	extern void AsmReloadGdtr(void* GdtBase, unsigned long GdtLimit);
	extern void AsmReloadIdtr(void* GdtBase, unsigned long GdtLimit);
}
