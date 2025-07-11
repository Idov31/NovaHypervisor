#pragma once

#include "pch.h"
#include "Spinlock.h"

#pragma warning(push)
#pragma warning(disable: 4201)

// Constants
constexpr ULONG64 MAX_PROTECTED_ADDRESSES = 512;

constexpr SIZE_T VMM_STACK_SIZE = 0x8000;
constexpr SIZE_T VMCS_SIZE = 4096;
constexpr SIZE_T VMXON_SIZE = 4096;
constexpr ULONG MSR_APIC_BASE = 0x01B;
constexpr ULONG MSR_IA32_FEATURE_CONTROL = 0x03A;
constexpr ULONG RPL_MASK = 3;

constexpr ULONG MSR_IA32_VMX_BASIC = 0x480;
constexpr ULONG MSR_IA32_VMX_PINBASED_CTLS = 0x481;
constexpr ULONG MSR_IA32_VMX_PROCBASED_CTLS = 0x482;
constexpr ULONG MSR_IA32_VMX_EXIT_CTLS = 0x483;
constexpr ULONG MSR_IA32_VMX_ENTRY_CTLS = 0x484;
constexpr ULONG MSR_IA32_VMX_MISC = 0x485;
constexpr ULONG MSR_IA32_VMX_CR0_FIXED0 = 0x486;
constexpr ULONG MSR_IA32_VMX_CR0_FIXED1 = 0x487;
constexpr ULONG MSR_IA32_VMX_CR4_FIXED0 = 0x488;
constexpr ULONG MSR_IA32_VMX_CR4_FIXED1 = 0x489;
constexpr ULONG MSR_IA32_VMX_VMCS_ENUM = 0x48A;
constexpr ULONG MSR_IA32_VMX_PROCBASED_CTLS2 = 0x48B;
constexpr ULONG MSR_IA32_VMX_EPT_VPID_CAP = 0x48C;
constexpr ULONG MSR_IA32_VMX_TRUE_PINBASED_CTLS = 0x48D;
constexpr ULONG MSR_IA32_VMX_TRUE_PROCBASED_CTLS = 0x48E;
constexpr ULONG MSR_IA32_VMX_TRUE_EXIT_CTLS = 0x48F;
constexpr ULONG MSR_IA32_VMX_TRUE_ENTRY_CTLS = 0x490;
constexpr ULONG MSR_IA32_VMX_VMFUNC = 0x491;

constexpr ULONG MSR_IA32_SYSENTER_CS = 0x174;
constexpr ULONG MSR_IA32_SYSENTER_ESP = 0x175;
constexpr ULONG MSR_IA32_SYSENTER_EIP = 0x176;
constexpr ULONG MSR_IA32_DEBUGCTL = 0x1D9;

constexpr ULONG MSR_LSTAR = 0xC0000082;

constexpr ULONG MSR_FS_BASE = 0xC0000100;
constexpr ULONG MSR_GS_BASE = 0xC0000101;
constexpr ULONG MSR_SHADOW_GS_BASE = 0xC0000102;

constexpr ULONG MSR_IA32_MTRR_DEF_TYPE = 0x2FF;
constexpr ULONG MSR_IA32_MTRR_CAPABILITIES = 0x000000FE;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE0 = 0x00000200;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE1 = 0x00000202;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE2 = 0x00000204;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE3 = 0x00000206;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE4 = 0x00000208;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE5 = 0x0000020A;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE6 = 0x0000020C;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE7 = 0x0000020E;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE8 = 0x00000210;
constexpr ULONG MSR_IA32_MTRR_PHYSBASE9 = 0x00000212;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK0 = 0x00000201;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK1 = 0x00000203;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK2 = 0x00000205;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK3 = 0x00000207;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK4 = 0x00000209;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK5 = 0x0000020B;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK6 = 0x0000020D;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK7 = 0x0000020F;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK8 = 0x00000211;
constexpr ULONG MSR_IA32_MTRR_PHYSMASK9 = 0x00000213;
constexpr SIZE_T IA32_MTRR_FIX64K_BASE = 0x00000000;
constexpr SIZE_T IA32_MTRR_FIX64K_SIZE = 0x00010000;
constexpr SIZE_T IA32_MTRR_FIX64K_00000 = 0x00000250;

constexpr SIZE_T IA32_MTRR_FIX16K_BASE = 0x00080000;
constexpr SIZE_T IA32_MTRR_FIX16K_SIZE = 0x00004000;
constexpr SIZE_T IA32_MTRR_FIX16K_80000 = 0x00000258;
constexpr SIZE_T IA32_MTRR_FIX16K_A0000 = 0x00000259;

constexpr SIZE_T IA32_MTRR_FIX4K_BASE = 0x000C0000;
constexpr SIZE_T IA32_MTRR_FIX4K_SIZE = 0x00001000;
constexpr SIZE_T IA32_MTRR_FIX4K_C0000 = 0x00000268;
constexpr SIZE_T IA32_MTRR_FIX4K_C8000 = 0x00000269;
constexpr SIZE_T IA32_MTRR_FIX4K_D0000 = 0x0000026A;
constexpr SIZE_T IA32_MTRR_FIX4K_D8000 = 0x0000026B;
constexpr SIZE_T IA32_MTRR_FIX4K_E0000 = 0x0000026C;
constexpr SIZE_T IA32_MTRR_FIX4K_E8000 = 0x0000026D;
constexpr SIZE_T IA32_MTRR_FIX4K_F0000 = 0x0000026E;
constexpr SIZE_T IA32_MTRR_FIX4K_F8000 = 0x0000026F;

constexpr ULONG64 RESERVED_MSR_RANGE_LOW = 0x40000000;
constexpr ULONG64 RESERVED_MSR_RANGE_HIGH = 0x400000F0;

constexpr USHORT VMEXIT_STATE_BITS = 0xF8;
constexpr ULONGLONG NO_ADDITIONAL_VMCS = ~0ULL;

constexpr ULONG PIN_BASED_VM_EXECUTION_CONTROLS_EXTERNAL_INTERRUPT = 0x00000001;
constexpr ULONG PIN_BASED_VM_EXECUTION_CONTROLS_NMI_EXITING = 0x00000008;
constexpr ULONG PIN_BASED_VM_EXECUTION_CONTROLS_VIRTUAL_NMI = 0x00000020;
constexpr ULONG PIN_BASED_VM_EXECUTION_CONTROLS_ACTIVE_VMX_TIMER = 0x00000040;
constexpr ULONG PIN_BASED_VM_EXECUTION_CONTROLS_PROCESS_POSTED_INTERRUPTS = 0x00000080;

constexpr ULONG CPU_BASED_VIRTUAL_INTR_PENDING = 0x00000004;
constexpr ULONG CPU_BASED_USE_TSC_OFFSETING = 0x00000008;
constexpr ULONG CPU_BASED_HLT_EXITING = 0x00000080;
constexpr ULONG CPU_BASED_INVLPG_EXITING = 0x00000200;
constexpr ULONG CPU_BASED_MWAIT_EXITING = 0x00000400;
constexpr ULONG CPU_BASED_RDPMC_EXITING = 0x00000800;
constexpr ULONG CPU_BASED_RDTSC_EXITING = 0x00001000;
constexpr ULONG CPU_BASED_CR3_LOAD_EXITING = 0x00008000;
constexpr ULONG CPU_BASED_CR3_STORE_EXITING = 0x00010000;
constexpr ULONG CPU_BASED_CR8_LOAD_EXITING = 0x00080000;
constexpr ULONG CPU_BASED_CR8_STORE_EXITING = 0x00100000;
constexpr ULONG CPU_BASED_TPR_SHADOW = 0x00200000;
constexpr ULONG CPU_BASED_VIRTUAL_NMI_PENDING = 0x00400000;
constexpr ULONG CPU_BASED_MOV_DR_EXITING = 0x00800000;
constexpr ULONG CPU_BASED_UNCOND_IO_EXITING = 0x01000000;
constexpr ULONG CPU_BASED_ACTIVATE_IO_BITMAP = 0x02000000;
constexpr ULONG CPU_BASED_MONITOR_TRAP_FLAG = 0x08000000;
constexpr ULONG CPU_BASED_ACTIVATE_MSR_BITMAP = 0x10000000;
constexpr ULONG CPU_BASED_MONITOR_EXITING = 0x20000000;
constexpr ULONG CPU_BASED_PAUSE_EXITING = 0x40000000;
constexpr ULONG CPU_BASED_ACTIVATE_SECONDARY_CONTROLS = 0x80000000;

constexpr ULONG CPU_BASED_CTL2_ENABLE_EPT = 0x2;
constexpr ULONG CPU_BASED_CTL2_RDTSCP = 0x8;
constexpr ULONG CPU_BASED_CTL2_ENABLE_VPID = 0x20;
constexpr ULONG CPU_BASED_CTL2_UNRESTRICTED_GUEST = 0x80;
constexpr ULONG CPU_BASED_CTL2_VIRTUAL_INTERRUPT_DELIVERY = 0x200;
constexpr ULONG CPU_BASED_CTL2_ENABLE_INVPCID = 0x1000;
constexpr ULONG CPU_BASED_CTL2_ENABLE_VMFUNC = 0x2000;
constexpr ULONG CPU_BASED_CTL2_ENABLE_XSAVE_XRSTORS = 0x100000;

// VMCalls
constexpr ULONG VMCALL_TEST = 1;
constexpr ULONG VMCALL_VMXOFF = 2;
constexpr ULONG VMCALL_EXEC_HOOK_PAGE = 3;
constexpr ULONG VMCALL_INVEPT_ALL_CONTEXT = 4;
constexpr ULONG VMCALL_INVEPT_SINGLE_CONTEXT = 5;
constexpr ULONG VMCALL_UNHOOK_SINGLE_PAGE = 6;
constexpr ULONG VMCALL_UNHOOK_ALL_PAGES = 7;

// VM-exit Control Bits
constexpr ULONG VM_EXIT_IA32E_MODE = 0x00000200;
constexpr ULONG VM_EXIT_ACK_INTR_ON_EXIT = 0x00008000;
constexpr ULONG VM_EXIT_SAVE_GUEST_PAT = 0x00040000;
constexpr ULONG VM_EXIT_LOAD_HOST_PAT = 0x00080000;

// VM-entry Control Bits
constexpr ULONG VM_ENTRY_IA32E_MODE = 0x00000200;
constexpr ULONG VM_ENTRY_SMM = 0x00000400;
constexpr ULONG VM_ENTRY_DEACT_DUAL_MONITOR = 0x00000800;
constexpr ULONG VM_ENTRY_LOAD_GUEST_PAT = 0x00004000;

// Exit Reasons
constexpr ULONG EXIT_REASON_EXCEPTION_NMI = 0;
constexpr ULONG EXIT_REASON_EXTERNAL_INTERRUPT = 1;
constexpr ULONG EXIT_REASON_TRIPLE_FAULT = 2;
constexpr ULONG EXIT_REASON_INIT = 3;
constexpr ULONG EXIT_REASON_SIPI = 4;
constexpr ULONG EXIT_REASON_IO_SMI = 5;
constexpr ULONG EXIT_REASON_OTHER_SMI = 6;
constexpr ULONG EXIT_REASON_PENDING_VIRT_INTR = 7;
constexpr ULONG EXIT_REASON_PENDING_VIRT_NMI = 8;
constexpr ULONG EXIT_REASON_TASK_SWITCH = 9;
constexpr ULONG EXIT_REASON_CPUID = 10;
constexpr ULONG EXIT_REASON_GETSEC = 11;
constexpr ULONG EXIT_REASON_HLT = 12;
constexpr ULONG EXIT_REASON_INVD = 13;
constexpr ULONG EXIT_REASON_INVLPG = 14;
constexpr ULONG EXIT_REASON_RDPMC = 15;
constexpr ULONG EXIT_REASON_RDTSC = 16;
constexpr ULONG EXIT_REASON_RSM = 17;
constexpr ULONG EXIT_REASON_VMCALL = 18;
constexpr ULONG EXIT_REASON_VMCLEAR = 19;
constexpr ULONG EXIT_REASON_VMLAUNCH = 20;
constexpr ULONG EXIT_REASON_VMPTRLD = 21;
constexpr ULONG EXIT_REASON_VMPTRST = 22;
constexpr ULONG EXIT_REASON_VMREAD = 23;
constexpr ULONG EXIT_REASON_VMRESUME = 24;
constexpr ULONG EXIT_REASON_VMWRITE = 25;
constexpr ULONG EXIT_REASON_VMXOFF = 26;
constexpr ULONG EXIT_REASON_VMXON = 27;
constexpr ULONG EXIT_REASON_CR_ACCESS = 28;
constexpr ULONG EXIT_REASON_DR_ACCESS = 29;
constexpr ULONG EXIT_REASON_IO_INSTRUCTION = 30;
constexpr ULONG EXIT_REASON_MSR_READ = 31;
constexpr ULONG EXIT_REASON_MSR_WRITE = 32;
constexpr ULONG EXIT_REASON_INVALID_GUEST_STATE = 33;
constexpr ULONG EXIT_REASON_MSR_LOADING = 34;
constexpr ULONG EXIT_REASON_MWAIT_INSTRUCTION = 36;
constexpr ULONG EXIT_REASON_MONITOR_TRAP_FLAG = 37;
constexpr ULONG EXIT_REASON_MONITOR_INSTRUCTION = 39;
constexpr ULONG EXIT_REASON_PAUSE_INSTRUCTION = 40;
constexpr ULONG EXIT_REASON_MCE_DURING_VMENTRY = 41;
constexpr ULONG EXIT_REASON_TPR_BELOW_THRESHOLD = 43;
constexpr ULONG EXIT_REASON_APIC_ACCESS = 44;
constexpr ULONG EXIT_REASON_ACCESS_GDTR_OR_IDTR = 46;
constexpr ULONG EXIT_REASON_ACCESS_LDTR_OR_TR = 47;
constexpr ULONG EXIT_REASON_EPT_VIOLATION = 48;
constexpr ULONG EXIT_REASON_EPT_MISCONFIG = 49;
constexpr ULONG EXIT_REASON_INVEPT = 50;
constexpr ULONG EXIT_REASON_RDTSCP = 51;
constexpr ULONG EXIT_REASON_VMX_PREEMPTION_TIMER_EXPIRED = 52;
constexpr ULONG EXIT_REASON_INVVPID = 53;
constexpr ULONG EXIT_REASON_WBINVD = 54;
constexpr ULONG EXIT_REASON_XSETBV = 55;
constexpr ULONG EXIT_REASON_APIC_WRITE = 56;
constexpr ULONG EXIT_REASON_RDRAND = 57;
constexpr ULONG EXIT_REASON_INVPCID = 58;
constexpr ULONG EXIT_REASON_RDSEED = 61;
constexpr ULONG EXIT_REASON_PML_FULL = 62;
constexpr ULONG EXIT_REASON_XSAVES = 63;
constexpr ULONG EXIT_REASON_XRSTORS = 64;
constexpr ULONG EXIT_REASON_PCOMMIT = 65;

// Hyperv cpuid
constexpr ULONG HYPERV_CPUID_VENDOR_AND_MAX_FUNCTIONS = 0x40000000;
constexpr ULONG HYPERV_CPUID_INTERFACE = 0x40000001;
constexpr ULONG HYPERV_CPUID_VERSION = 0x40000002;
constexpr ULONG HYPERV_CPUID_FEATURES = 0x40000003;
constexpr ULONG HYPERV_CPUID_ENLIGHTMENT_INFO = 0x40000004;
constexpr ULONG HYPERV_CPUID_IMPLEMENT_LIMITS = 0x40000005;
constexpr ULONG HYPERV_HYPERVISOR_PRESENT_BIT = 0x80000000;
constexpr ULONG HYPERV_CPUID_MIN = 0x40000005;
constexpr ULONG HYPERV_CPUID_MAX = 0x4000ffff;

// Exit qualification for CR accesses
constexpr ULONG TYPE_MOV_TO_CR = 0;
constexpr ULONG TYPE_MOV_FROM_CR = 1;
constexpr ULONG TYPE_CLTS = 2;
constexpr ULONG TYPE_LMSW = 3;

// DPL modes
constexpr SIZE_T DPL_USER = 3;
constexpr SIZE_T DPL_SYSTEM = 0;

// Ept definitions
constexpr SIZE_T VMM_EPT_PML4E_COUNT = 512;
constexpr SIZE_T VMM_EPT_PML3E_COUNT = 512;
constexpr SIZE_T VMM_EPT_PML2E_COUNT = 512;
constexpr SIZE_T VMM_EPT_PML1E_COUNT = 512;

constexpr UCHAR MEMORY_TYPE_UNCACHEABLE = 0x00000000;
constexpr UCHAR MEMORY_TYPE_WRITE_COMBINING = 0x00000001;
constexpr UCHAR MEMORY_TYPE_WRITE_THROUGH = 0x00000004;
constexpr UCHAR MEMORY_TYPE_WRITE_PROTECTED = 0x00000005;
constexpr UCHAR MEMORY_TYPE_WRITE_BACK = 0x00000006;
constexpr UCHAR MEMORY_TYPE_INVALID = 0x000000FF;

constexpr SIZE_T VPID_TAG = 1;

// Integer 2MB
constexpr SIZE_T SIZE_2_MB = ((SIZE_T)(512 * PAGE_SIZE));

// Offset into the 1st paging structure (4096 byte)
#define ADDRMASK_EPT_PML1_OFFSET(_VAR_) (_VAR_ & 0xFFFULL)

// Index of the 1st paging structure (4096 byte)
#define ADDRMASK_EPT_PML1_INDEX(_VAR_) ((_VAR_ & 0x1FF000ULL) >> 12)

// Index of the 2nd paging structure (2MB)
#define ADDRMASK_EPT_PML2_INDEX(_VAR_) ((_VAR_ & 0x3FE00000ULL) >> 21)

// Index of the 3rd paging structure (1GB)
#define ADDRMASK_EPT_PML3_INDEX(_VAR_) ((_VAR_ & 0x7FC0000000ULL) >> 30)

// Index of the 4th paging structure (512GB)
#define ADDRMASK_EPT_PML4_INDEX(_VAR_) ((_VAR_ & 0xFF8000000000ULL) >> 39)

constexpr ULONG64 CPUID_PROCESSOR_AND_PROCESSOR_FEATURE_IDENTIFIERS = 1;

// Type definitions
enum RegionType {
	VMXON_REGION,
	VMCS_REGION,
	MSR_BITMAP_REGION
};

typedef union _PEPT_PML4
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 512-GByte region controlled by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 512-GByte region controlled by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 512-GByte region controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 512-GByte region controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;
		UINT64 Reserved1 : 5;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 512-GByte region
		 * controlled by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;
		UINT64 Reserved2 : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 512-GByte region
		 * controlled by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved3 : 1;

		/**
		 * [Bits 47:12] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
		 */
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved4 : 16;
	};

	UINT64 Flags;
} EPT_PML4, * PEPT_PML4;



typedef union _EPDPTE_1GB
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 1-GByte page referenced by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 1-GByte page referenced by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 1-GByte page controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 1-GByte page controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;

		/**
		 * [Bits 5:3] EPT memory type for this 1-GByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 MemoryType : 3;

		/**
		 * [Bit 6] Ignore PAT memory type for this 1-GByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 IgnorePat : 1;

		/**
		 * [Bit 7] Must be 1 (otherwise, this entry references an EPT page directory).
		 */
		UINT64 LargePage : 1;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 1-GByte page
		 * referenced by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;

		/**
		 * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 1-GByte page referenced
		 * by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Dirty : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 1-GByte page controlled
		 * by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved1 : 19;

		/**
		 * [Bits 47:30] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
		 */
		UINT64 PageFrameNumber : 18;
		UINT64 Reserved2 : 15;

		/**
		 * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
		 * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
		 * 0, this bit is ignored.
		 *
		 * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
		 */
		UINT64 SuppressVe : 1;
	};

	UINT64 Flags;
} EPDPTE_1GB, * PEPDPTE_1GB;



typedef union _EPDPTE
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 1-GByte region controlled by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 1-GByte region controlled by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 1-GByte region controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 1-GByte region controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;
		UINT64 Reserved1 : 5;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 1-GByte region
		 * controlled by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;
		UINT64 Reserved2 : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 1-GByte region controlled
		 * by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved3 : 1;

		/**
		 * [Bits 47:12] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
		 */
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved4 : 16;
	};

	UINT64 Flags;
} EPDPTE, * PEPDPTE;


typedef union _EPDE_2MB
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 2-MByte page referenced by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 2-MByte page referenced by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 2-MByte page controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 2-MByte page controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;

		/**
		 * [Bits 5:3] EPT memory type for this 2-MByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 MemoryType : 3;

		/**
		 * [Bit 6] Ignore PAT memory type for this 2-MByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 IgnorePat : 1;

		/**
		 * [Bit 7] Must be 1 (otherwise, this entry references an EPT page table).
		 */
		UINT64 LargePage : 1;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 2-MByte page
		 * referenced by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;

		/**
		 * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 2-MByte page referenced
		 * by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Dirty : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 2-MByte page controlled
		 * by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved1 : 10;

		/**
		 * [Bits 47:21] Physical address of 4-KByte aligned EPT page-directory-pointer table referenced by this entry.
		 */
		UINT64 PageFrameNumber : 27;
		UINT64 Reserved2 : 15;

		/**
		 * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
		 * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
		 * 0, this bit is ignored.
		 *
		 * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
		 */
		UINT64 SuppressVe : 1;
	};

	UINT64 Flags;
} EPDE_2MB, * PEPDE_2MB;



typedef union _EPDE
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 2-MByte region controlled by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 2-MByte region controlled by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 2-MByte region controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 2-MByte region controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;
		UINT64 Reserved1 : 5;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 2-MByte region
		 * controlled by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;
		UINT64 Reserved2 : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 2-MByte region controlled
		 * by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved3 : 1;

		/**
		 * [Bits 47:12] Physical address of 4-KByte aligned EPT page table referenced by this entry.
		 */
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved4 : 16;
	};

	UINT64 Flags;
} EPDE, * PEPDE;


typedef union _EPTE
{
	struct
	{
		/**
		 * [Bit 0] Read access; indicates whether reads are allowed from the 4-KByte page referenced by this entry.
		 */
		UINT64 ReadAccess : 1;

		/**
		 * [Bit 1] Write access; indicates whether writes are allowed from the 4-KByte page referenced by this entry.
		 */
		UINT64 WriteAccess : 1;

		/**
		 * [Bit 2] If the "mode-based execute control for EPT" VM-execution control is 0, execute access; indicates whether
		 * instruction fetches are allowed from the 4-KByte page controlled by this entry.
		 * If that control is 1, execute access for supervisor-mode linear addresses; indicates whether instruction fetches are
		 * allowed from supervisor-mode linear addresses in the 4-KByte page controlled by this entry.
		 */
		UINT64 ExecuteAccess : 1;

		/**
		 * [Bits 5:3] EPT memory type for this 4-KByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 MemoryType : 3;

		/**
		 * [Bit 6] Ignore PAT memory type for this 4-KByte page.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 IgnorePat : 1;
		UINT64 Reserved1 : 1;

		/**
		 * [Bit 8] If bit 6 of EPTP is 1, accessed flag for EPT; indicates whether software has accessed the 4-KByte page
		 * referenced by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Accessed : 1;

		/**
		 * [Bit 9] If bit 6 of EPTP is 1, dirty flag for EPT; indicates whether software has written to the 4-KByte page referenced
		 * by this entry. Ignored if bit 6 of EPTP is 0.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 Dirty : 1;

		/**
		 * [Bit 10] Execute access for user-mode linear addresses. If the "mode-based execute control for EPT" VM-execution control
		 * is 1, indicates whether instruction fetches are allowed from user-mode linear addresses in the 4-KByte page controlled
		 * by this entry. If that control is 0, this bit is ignored.
		 */
		UINT64 UserModeExecute : 1;
		UINT64 Reserved2 : 1;

		/**
		 * [Bits 47:12] Physical address of the 4-KByte page referenced by this entry.
		 */
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved3 : 15;

		/**
		 * [Bit 63] Suppress \#VE. If the "EPT-violation \#VE" VM-execution control is 1, EPT violations caused by accesses to this
		 * page are convertible to virtualization exceptions only if this bit is 0. If "EPT-violation \#VE" VMexecution control is
		 * 0, this bit is ignored.
		 *
		 * @see Vol3C[25.5.6.1(Convertible EPT Violations)]
		 */
		UINT64 SuppressVe : 1;
	};

	UINT64 Flags;
} EPTE, * PEPTE;

typedef EPT_PML4 EPT_PML4_POINTER, * PEPT_PML4_POINTER;
typedef EPDPTE EPT_PML3_POINTER, * PEPT_PML3_POINTER;
typedef EPDE_2MB EPT_PML2_ENTRY, * PEPT_PML2_ENTRY;
typedef EPDE EPT_PML2_POINTER, * PEPT_PML2_POINTER;
typedef EPTE EPT_PML1_ENTRY, * PEPT_PML1_ENTRY;

typedef struct _VMM_EPT_PAGE_TABLE
{
	DECLSPEC_ALIGN(PAGE_SIZE) EPT_PML4_POINTER PML4[VMM_EPT_PML4E_COUNT];
	DECLSPEC_ALIGN(PAGE_SIZE) EPT_PML3_POINTER PML3[VMM_EPT_PML3E_COUNT];
	DECLSPEC_ALIGN(PAGE_SIZE) EPT_PML2_ENTRY PML2[VMM_EPT_PML3E_COUNT][VMM_EPT_PML2E_COUNT];
} VMM_EPT_PAGE_TABLE, * PVMM_EPT_PAGE_TABLE;

typedef struct _VMM_EPT_DYNAMIC_SPLIT
{
	DECLSPEC_ALIGN(PAGE_SIZE) EPT_PML1_ENTRY PML1[VMM_EPT_PML1E_COUNT];
	union
	{
		PEPT_PML2_ENTRY Entry;
		PEPT_PML2_POINTER Pointer;
	};

} VMM_EPT_DYNAMIC_SPLIT, * PVMM_EPT_DYNAMIC_SPLIT;

typedef struct _MTRR_RANGE_DESCRIPTOR
{
	SIZE_T PhysicalBaseAddress;
	SIZE_T PhysicalEndAddress;
	UCHAR MemoryType;
	bool FixedRange;
} MTRR_RANGE_DESCRIPTOR, * PMTRR_RANGE_DESCRIPTOR;

typedef union _IA32_VMX_EPT_VPID_CAP_REGISTER
{
	struct
	{
		/**
		 * [Bit 0] When set to 1, the processor supports execute-only translations by EPT. This support allows software to
		 * configure EPT paging-structure entries in which bits 1:0 are clear (indicating that data accesses are not allowed) and
		 * bit 2 is set (indicating that instruction fetches are allowed).
		 */
		UINT64 ExecuteOnlyPages : 1;
		UINT64 Reserved1 : 5;

		/**
		 * [Bit 6] Indicates support for a page-walk length of 4.
		 */
		UINT64 PageWalkLength4 : 1;
		UINT64 Reserved2 : 1;

		/**
		 * [Bit 8] When set to 1, the logical processor allows software to configure the EPT paging-structure memory type to be
		 * uncacheable (UC).
		 *
		 * @see Vol3C[24.6.11(Extended-Page-Table Pointer (EPTP))]
		 */
		UINT64 MemoryTypeUncacheable : 1;
		UINT64 Reserved3 : 5;

		/**
		 * [Bit 14] When set to 1, the logical processor allows software to configure the EPT paging-structure memory type to be
		 * write-back (WB).
		 */
		UINT64 MemoryTypeWriteBack : 1;
		UINT64 Reserved4 : 1;

		/**
		 * [Bit 16] When set to 1, the logical processor allows software to configure a EPT PDE to map a 2-Mbyte page (by setting
		 * bit 7 in the EPT PDE).
		 */
		UINT64 Pde2MbPages : 1;

		/**
		 * [Bit 17] When set to 1, the logical processor allows software to configure a EPT PDPTE to map a 1-Gbyte page (by setting
		 * bit 7 in the EPT PDPTE).
		 */
		UINT64 Pdpte1GbPages : 1;
		UINT64 Reserved5 : 2;

		/**
		 * [Bit 20] If bit 20 is read as 1, the INVEPT instruction is supported.
		 *
		 * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
		 * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
		 */
		UINT64 Invept : 1;

		/**
		 * [Bit 21] When set to 1, accessed and dirty flags for EPT are supported.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 EptAccessedAndDirtyFlags : 1;

		/**
		 * [Bit 22] When set to 1, the processor reports advanced VM-exit information for EPT violations. This reporting is done
		 * only if this bit is read as 1.
		 *
		 * @see Vol3C[27.2.1(Basic VM-Exit Information)]
		 */
		UINT64 AdvancedVmexitEptViolationsInformation : 1;
		UINT64 Reserved6 : 2;

		/**
		 * [Bit 25] When set to 1, the single-context INVEPT type is supported.
		 *
		 * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
		 * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
		 */
		UINT64 InveptSingleContext : 1;

		/**
		 * [Bit 26] When set to 1, the all-context INVEPT type is supported.
		 *
		 * @see Vol3C[30(VMX INSTRUCTION REFERENCE)]
		 * @see Vol3C[28.3.3.1(Operations that Invalidate Cached Mappings)]
		 */
		UINT64 InveptAllContexts : 1;
		UINT64 Reserved7 : 5;

		/**
		 * [Bit 32] When set to 1, the INVVPID instruction is supported.
		 */
		UINT64 Invvpid : 1;
		UINT64 Reserved8 : 7;

		/**
		 * [Bit 40] When set to 1, the individual-address INVVPID type is supported.
		 */
		UINT64 InvvpidIndividualAddress : 1;

		/**
		 * [Bit 41] When set to 1, the single-context INVVPID type is supported.
		 */
		UINT64 InvvpidSingleContext : 1;

		/**
		 * [Bit 42] When set to 1, the all-context INVVPID type is supported.
		 */
		UINT64 InvvpidAllContexts : 1;

		/**
		 * [Bit 43] When set to 1, the single-context-retaining-globals INVVPID type is supported.
		 */
		UINT64 InvvpidSingleContextRetainGlobals : 1;
		UINT64 Reserved9 : 20;
	};

	UINT64 Flags;
} IA32_VMX_EPT_VPID_CAP_REGISTER, * PIA32_VMX_EPT_VPID_CAP_REGISTER;

// MSR_IA32_MTRR_DEF_TYPE 
typedef union
{
	struct
	{
		/**
		 * [Bits 2:0] Default Memory Type.
		 */
		UINT64 DefaultMemoryType : 3;
#define IA32_MTRR_DEF_TYPE_DEFAULT_MEMORY_TYPE_BIT                   0
#define IA32_MTRR_DEF_TYPE_DEFAULT_MEMORY_TYPE_FLAG                  0x07
#define IA32_MTRR_DEF_TYPE_DEFAULT_MEMORY_TYPE_MASK                  0x07
#define IA32_MTRR_DEF_TYPE_DEFAULT_MEMORY_TYPE(_)                    (((_) >> 0) & 0x07)
		UINT64 Reserved1 : 7;

		/**
		 * [Bit 10] Fixed Range MTRR Enable.
		 */
		UINT64 FixedRangeMtrrEnable : 1;
#define IA32_MTRR_DEF_TYPE_FIXED_RANGE_MTRR_ENABLE_BIT               10
#define IA32_MTRR_DEF_TYPE_FIXED_RANGE_MTRR_ENABLE_FLAG              0x400
#define IA32_MTRR_DEF_TYPE_FIXED_RANGE_MTRR_ENABLE_MASK              0x01
#define IA32_MTRR_DEF_TYPE_FIXED_RANGE_MTRR_ENABLE(_)                (((_) >> 10) & 0x01)

		/**
		 * [Bit 11] MTRR Enable.
		 */
		UINT64 MtrrEnable : 1;
#define IA32_MTRR_DEF_TYPE_MTRR_ENABLE_BIT                           11
#define IA32_MTRR_DEF_TYPE_MTRR_ENABLE_FLAG                          0x800
#define IA32_MTRR_DEF_TYPE_MTRR_ENABLE_MASK                          0x01
#define IA32_MTRR_DEF_TYPE_MTRR_ENABLE(_)                            (((_) >> 11) & 0x01)
		UINT64 Reserved2 : 52;
	};

	UINT64 Flags;
} IA32_MTRR_DEF_TYPE_REGISTER;

typedef union _IA32_FEATURE_CONTROL_MSR
{
	ULONG64 All;
	struct
	{
		ULONG64 Lock : 1;
		ULONG64 EnableSMX : 1;
		ULONG64 EnableVmxon : 1;
		ULONG64 Reserved2 : 5;
		ULONG64 EnableLocalSENTER : 7;
		ULONG64 EnableGlobalSENTER : 1;
		ULONG64 Reserved3a : 16;
		ULONG64 Reserved3b : 32;
	} Fields;
} IA32_FEATURE_CONTROL_MSR, * PIA32_FEATURE_CONTROL_MSR;

typedef struct _CPUID {
	int eax;
	int ebx;
	int ecx;
	int edx;
} CPUID, * PCPUID;

typedef union _IA32_VMX_BASIC_MSR
{
	ULONG64 All;
	struct
	{
		ULONG32 RevisionIdentifier : 31;  // [0-30]
		ULONG32 Reserved1 : 1;            // [31]
		ULONG32 RegionSize : 12;          // [32-43]
		ULONG32 RegionClear : 1;          // [44]
		ULONG32 Reserved2 : 3;            // [45-47]
		ULONG32 SupportedIA64 : 1;        // [48]
		ULONG32 SupportedDualMoniter : 1; // [49]
		ULONG32 MemoryType : 4;           // [50-53]
		ULONG32 VmExitReport : 1;         // [54]
		ULONG32 VmxCapabilityHint : 1;    // [55]
		ULONG32 Reserved3 : 8;            // [56-63]
	} Fields;
} IA32_VMX_BASIC_MSR, * PIA32_VMX_BASIC_MSR;

typedef union _EPTP
{
	struct
	{
		/**
		 * [Bits 2:0] EPT paging-structure memory type:
		 * - 0 = Uncacheable (UC)
		 * - 6 = Write-back (WB)
		 * Other values are reserved.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 MemoryType : 3;

		/**
		 * [Bits 5:3] This value is 1 less than the EPT page-walk length.
		 *
		 * @see Vol3C[28.2.6(EPT and memory Typing)]
		 */
		UINT64 PageWalkLength : 3;

		/**
		 * [Bit 6] Setting this control to 1 enables accessed and dirty flags for EPT.
		 *
		 * @see Vol3C[28.2.4(Accessed and Dirty Flags for EPT)]
		 */
		UINT64 EnableAccessAndDirtyFlags : 1;
		UINT64 Reserved1 : 5;

		/**
		 * [Bits 47:12] Bits N-1:12 of the physical address of the 4-KByte aligned EPT PML4 table.
		 */
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved2 : 16;
	};

	UINT64 Flags;
} EPTP, * PEPTP;

typedef union _VMX_EXIT_QUALIFICATION_EPT_VIOLATION
{
	struct
	{
		UINT64 ReadAccess : 1;
		UINT64 WriteAccess : 1;
		UINT64 ExecuteAccess : 1;
		UINT64 EptReadable : 1;
		UINT64 EptWriteable : 1;
		UINT64 EptExecutable : 1;
		UINT64 EptExecutableForUserMode : 1;
		UINT64 ValidGuestLinearAddress : 1;
		UINT64 CausedByTranslation : 1;
		UINT64 UserModeLinearAddress : 1;
		UINT64 ReadableWritablePage : 1;
		UINT64 ExecuteDisablePage : 1;
		UINT64 NmiUnblocking : 1;
		UINT64 Reserved1 : 51;
	};

	UINT64 Flags;
} VMX_EXIT_QUALIFICATION_EPT_VIOLATION, * PVMX_EXIT_QUALIFICATION_EPT_VIOLATION;

//
// See Table 28-1.
//
typedef union _EPT_PML4E
{
	ULONG64 All;
	struct
	{
		UINT64 Read : 1;               // bit 0
		UINT64 Write : 1;              // bit 1
		UINT64 Execute : 1;            // bit 2
		UINT64 Reserved1 : 5;          // bit 7:3 (Must be Zero)
		UINT64 Accessed : 1;           // bit 8
		UINT64 Ignored1 : 1;           // bit 9
		UINT64 ExecuteForUserMode : 1; // bit 10
		UINT64 Ignored2 : 1;           // bit 11
		UINT64 PhysicalAddress : 36;   // bit (N-1):12 or Page-Frame-Number
		UINT64 Reserved2 : 4;          // bit 51:N
		UINT64 Ignored3 : 12;          // bit 63:52
	} Fields;
} EPT_PML4E, * PEPT_PML4E;

//
// See Table 28-3
//
typedef union _EPT_PDPTE
{
	ULONG64 All;
	struct
	{
		UINT64 Read : 1;               // bit 0
		UINT64 Write : 1;              // bit 1
		UINT64 Execute : 1;            // bit 2
		UINT64 Reserved1 : 5;          // bit 7:3 (Must be Zero)
		UINT64 Accessed : 1;           // bit 8
		UINT64 Ignored1 : 1;           // bit 9
		UINT64 ExecuteForUserMode : 1; // bit 10
		UINT64 Ignored2 : 1;           // bit 11
		UINT64 PhysicalAddress : 36;   // bit (N-1):12 or Page-Frame-Number
		UINT64 Reserved2 : 4;          // bit 51:N
		UINT64 Ignored3 : 12;          // bit 63:52
	} Fields;
} EPT_PDPTE, * PEPT_PDPTE;

//
// See Table 28-5
//
typedef union _EPT_PDE
{
	ULONG64 All;
	struct
	{
		UINT64 Read : 1;               // bit 0
		UINT64 Write : 1;              // bit 1
		UINT64 Execute : 1;            // bit 2
		UINT64 Reserved1 : 5;          // bit 7:3 (Must be Zero)
		UINT64 Accessed : 1;           // bit 8
		UINT64 Ignored1 : 1;           // bit 9
		UINT64 ExecuteForUserMode : 1; // bit 10
		UINT64 Ignored2 : 1;           // bit 11
		UINT64 PhysicalAddress : 36;   // bit (N-1):12 or Page-Frame-Number
		UINT64 Reserved2 : 4;          // bit 51:N
		UINT64 Ignored3 : 12;          // bit 63:52
	} Fields;
} EPT_PDE, * PEPT_PDE;

//
// See Table 28-6
//
typedef union _EPT_PTE
{
	ULONG64 All;
	struct
	{
		UINT64 Read : 1;               // bit 0
		UINT64 Write : 1;              // bit 1
		UINT64 Execute : 1;            // bit 2
		UINT64 EPTMemoryType : 3;      // bit 5:3 (EPT Memory type)
		UINT64 IgnorePAT : 1;          // bit 6
		UINT64 Ignored1 : 1;           // bit 7
		UINT64 AccessedFlag : 1;       // bit 8
		UINT64 DirtyFlag : 1;          // bit 9
		UINT64 ExecuteForUserMode : 1; // bit 10
		UINT64 Ignored2 : 1;           // bit 11
		UINT64 PhysicalAddress : 36;   // bit (N-1):12 or Page-Frame-Number
		UINT64 Reserved : 4;           // bit 51:N
		UINT64 Ignored3 : 11;          // bit 62:52
		UINT64 SuppressVE : 1;         // bit 63
	} Fields;
} EPT_PTE, * PEPT_PTE;

typedef union _IA32_MTRR_CAPABILITIES_REGISTER
{
	struct
	{
		UINT64 VariableRangeCount : 8;
		UINT64 FixedRangeSupported : 1;
		UINT64 Reserved1 : 1;
		UINT64 WcSupported : 1;
		UINT64 SmrrSupported : 1;
		UINT64 Reserved2 : 52;
	};

	UINT64 Flags;
} IA32_MTRR_CAPABILITIES_REGISTER, * PIA32_MTRR_CAPABILITIES_REGISTER;


// MSR_IA32_MTRR_PHYSBASE(0-9)
typedef union _IA32_MTRR_PHYSBASE_REGISTER
{
	struct
	{
		UINT64 Type : 8;
		UINT64 Reserved1 : 4;
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved2 : 16;
	};

	UINT64 Flags;
} IA32_MTRR_PHYSBASE_REGISTER, * PIA32_MTRR_PHYSBASE_REGISTER;


typedef union _IA32_MTRR_PHYSMASK_REGISTER
{
	struct
	{
		UINT64 Type : 8;
		UINT64 Reserved1 : 3;
		UINT64 Valid : 1;
		UINT64 PageFrameNumber : 36;
		UINT64 Reserved2 : 16;
	};

	UINT64 Flags;
} IA32_MTRR_PHYSMASK_REGISTER, * PIA32_MTRR_PHYSMASK_REGISTER;

enum INVEPT_TYPE
{
	SINGLE_CONTEXT = 0x00000001,
	ALL_CONTEXTS = 0x00000002,
};

enum VMCS_FIELDS
{
	VIRTUAL_PROCESSOR_ID = 0x00000000,
	GUEST_ES_SELECTOR = 0x00000800,
	GUEST_CS_SELECTOR = 0x00000802,
	GUEST_SS_SELECTOR = 0x00000804,
	GUEST_DS_SELECTOR = 0x00000806,
	GUEST_FS_SELECTOR = 0x00000808,
	GUEST_GS_SELECTOR = 0x0000080a,
	GUEST_LDTR_SELECTOR = 0x0000080c,
	GUEST_TR_SELECTOR = 0x0000080e,
	HOST_ES_SELECTOR = 0x00000c00,
	HOST_CS_SELECTOR = 0x00000c02,
	HOST_SS_SELECTOR = 0x00000c04,
	HOST_DS_SELECTOR = 0x00000c06,
	HOST_FS_SELECTOR = 0x00000c08,
	HOST_GS_SELECTOR = 0x00000c0a,
	HOST_TR_SELECTOR = 0x00000c0c,
	IO_BITMAP_A = 0x00002000,
	IO_BITMAP_A_HIGH = 0x00002001,
	IO_BITMAP_B = 0x00002002,
	IO_BITMAP_B_HIGH = 0x00002003,
	MSR_BITMAP = 0x00002004,
	MSR_BITMAP_HIGH = 0x00002005,
	VM_EXIT_MSR_STORE_ADDR = 0x00002006,
	VM_EXIT_MSR_STORE_ADDR_HIGH = 0x00002007,
	VM_EXIT_MSR_LOAD_ADDR = 0x00002008,
	VM_EXIT_MSR_LOAD_ADDR_HIGH = 0x00002009,
	VM_ENTRY_MSR_LOAD_ADDR = 0x0000200a,
	VM_ENTRY_MSR_LOAD_ADDR_HIGH = 0x0000200b,
	TSC_OFFSET = 0x00002010,
	TSC_OFFSET_HIGH = 0x00002011,
	VIRTUAL_APIC_PAGE_ADDR = 0x00002012,
	VIRTUAL_APIC_PAGE_ADDR_HIGH = 0x00002013,
	VMFUNC_CONTROLS = 0x00002018,
	VMFUNC_CONTROLS_HIGH = 0x00002019,
	EPT_POINTER = 0x0000201A,
	EPT_POINTER_HIGH = 0x0000201B,
	EPTP_LIST = 0x00002024,
	EPTP_LIST_HIGH = 0x00002025,
	GUEST_PHYSICAL_ADDRESS = 0x2400,
	GUEST_PHYSICAL_ADDRESS_HIGH = 0x2401,
	VMCS_LINK_POINTER = 0x00002800,
	VMCS_LINK_POINTER_HIGH = 0x00002801,
	GUEST_IA32_DEBUGCTL = 0x00002802,
	GUEST_IA32_DEBUGCTL_HIGH = 0x00002803,
	PIN_BASED_VM_EXEC_CONTROL = 0x00004000,
	CPU_BASED_VM_EXEC_CONTROL = 0x00004002,
	EXCEPTION_BITMAP = 0x00004004,
	PAGE_FAULT_ERROR_CODE_MASK = 0x00004006,
	PAGE_FAULT_ERROR_CODE_MATCH = 0x00004008,
	CR3_TARGET_COUNT = 0x0000400a,
	VM_EXIT_CONTROLS = 0x0000400c,
	VM_EXIT_MSR_STORE_COUNT = 0x0000400e,
	VM_EXIT_MSR_LOAD_COUNT = 0x00004010,
	VM_ENTRY_CONTROLS = 0x00004012,
	VM_ENTRY_MSR_LOAD_COUNT = 0x00004014,
	VM_ENTRY_INTR_INFO_FIELD = 0x00004016,
	VM_ENTRY_EXCEPTION_ERROR_CODE = 0x00004018,
	VM_ENTRY_INSTRUCTION_LEN = 0x0000401a,
	TPR_THRESHOLD = 0x0000401c,
	SECONDARY_VM_EXEC_CONTROL = 0x0000401e,
	VM_INSTRUCTION_ERROR = 0x00004400,
	VM_EXIT_REASON = 0x00004402,
	VM_EXIT_INTR_INFO = 0x00004404,
	VM_EXIT_INTR_ERROR_CODE = 0x00004406,
	IDT_VECTORING_INFO_FIELD = 0x00004408,
	IDT_VECTORING_ERROR_CODE = 0x0000440a,
	VM_EXIT_INSTRUCTION_LEN = 0x0000440c,
	VMX_INSTRUCTION_INFO = 0x0000440e,
	GUEST_ES_LIMIT = 0x00004800,
	GUEST_CS_LIMIT = 0x00004802,
	GUEST_SS_LIMIT = 0x00004804,
	GUEST_DS_LIMIT = 0x00004806,
	GUEST_FS_LIMIT = 0x00004808,
	GUEST_GS_LIMIT = 0x0000480a,
	GUEST_LDTR_LIMIT = 0x0000480c,
	GUEST_TR_LIMIT = 0x0000480e,
	GUEST_GDTR_LIMIT = 0x00004810,
	GUEST_IDTR_LIMIT = 0x00004812,
	GUEST_ES_AR_BYTES = 0x00004814,
	GUEST_CS_AR_BYTES = 0x00004816,
	GUEST_SS_AR_BYTES = 0x00004818,
	GUEST_DS_AR_BYTES = 0x0000481a,
	GUEST_FS_AR_BYTES = 0x0000481c,
	GUEST_GS_AR_BYTES = 0x0000481e,
	GUEST_LDTR_AR_BYTES = 0x00004820,
	GUEST_TR_AR_BYTES = 0x00004822,
	GUEST_INTERRUPTIBILITY_INFO = 0x00004824,
	GUEST_ACTIVITY_STATE = 0x00004826,
	GUEST_SM_BASE = 0x00004828,
	GUEST_SYSENTER_CS = 0x0000482A,
	HOST_IA32_SYSENTER_CS = 0x00004c00,
	CR0_GUEST_HOST_MASK = 0x00006000,
	CR4_GUEST_HOST_MASK = 0x00006002,
	CR0_READ_SHADOW = 0x00006004,
	CR4_READ_SHADOW = 0x00006006,
	CR3_TARGET_VALUE0 = 0x00006008,
	CR3_TARGET_VALUE1 = 0x0000600a,
	CR3_TARGET_VALUE2 = 0x0000600c,
	CR3_TARGET_VALUE3 = 0x0000600e,
	EXIT_QUALIFICATION = 0x00006400,
	GUEST_LINEAR_ADDRESS = 0x0000640a,
	GUEST_CR0 = 0x00006800,
	GUEST_CR3 = 0x00006802,
	GUEST_CR4 = 0x00006804,
	GUEST_ES_BASE = 0x00006806,
	GUEST_CS_BASE = 0x00006808,
	GUEST_SS_BASE = 0x0000680a,
	GUEST_DS_BASE = 0x0000680c,
	GUEST_FS_BASE = 0x0000680e,
	GUEST_GS_BASE = 0x00006810,
	GUEST_LDTR_BASE = 0x00006812,
	GUEST_TR_BASE = 0x00006814,
	GUEST_GDTR_BASE = 0x00006816,
	GUEST_IDTR_BASE = 0x00006818,
	GUEST_DR7 = 0x0000681a,
	GUEST_RSP = 0x0000681c,
	GUEST_RIP = 0x0000681e,
	GUEST_RFLAGS = 0x00006820,
	GUEST_PENDING_DBG_EXCEPTIONS = 0x00006822,
	GUEST_SYSENTER_ESP = 0x00006824,
	GUEST_SYSENTER_EIP = 0x00006826,
	HOST_CR0 = 0x00006c00,
	HOST_CR3 = 0x00006c02,
	HOST_CR4 = 0x00006c04,
	HOST_FS_BASE = 0x00006c06,
	HOST_GS_BASE = 0x00006c08,
	HOST_TR_BASE = 0x00006c0a,
	HOST_GDTR_BASE = 0x00006c0c,
	HOST_IDTR_BASE = 0x00006c0e,
	HOST_IA32_SYSENTER_ESP = 0x00006c10,
	HOST_IA32_SYSENTER_EIP = 0x00006c12,
	HOST_RSP = 0x00006c14,
	HOST_RIP = 0x00006c16,
};

typedef struct _INVEPT_DESCRIPTOR
{
	UINT64 EptPointer;
	UINT64 Reserved; // Must be zero.
} INVEPT_DESCRIPTOR, * PINVEPT_DESCRIPTOR;

typedef struct _INVEPT_DESC
{
	EPTP EptPointer;
	UINT64  Reserveds;
}INVEPT_DESC, * PINVEPT_DESC;

typedef enum _InvvpidType
{
	InvvpidIndividualAddress = 0x00000000,
	InvvpidSingleContext = 0x00000001,
	InvvpidAllContext = 0x00000002,
	InvvpidSingleContextRetainingGlobals = 0x00000003
} InvvpidType;

typedef struct _INVVPID_DESCRIPTOR {
	UINT64 Vpid : 16;
	UINT64 Reserved : 48;
	UINT64 LinearAddress;
} INVVPID_DESCRIPTOR, * PINVVPID_DESCRIPTOR;

typedef struct _GUEST_REGS
{
	ULONG64 rax;                  // 0x00         
	ULONG64 rcx;
	ULONG64 rdx;                  // 0x10
	ULONG64 rbx;
	ULONG64 rsp;                  // 0x20         // rsp is not stored here
	ULONG64 rbp;
	ULONG64 rsi;                  // 0x30
	ULONG64 rdi;
	ULONG64 r8;                   // 0x40
	ULONG64 r9;
	ULONG64 r10;                  // 0x50
	ULONG64 r11;
	ULONG64 r12;                  // 0x60
	ULONG64 r13;
	ULONG64 r14;                  // 0x70
	ULONG64 r15;
} GUEST_REGS, * PGUEST_REGS;

typedef union SEGMENT_ATTRIBUTES
{
	USHORT UCHARs;
	struct
	{
		USHORT TYPE : 4; /* 0;  Bit 40-43 */
		USHORT S : 1;    /* 4;  Bit 44 */
		USHORT DPL : 2;  /* 5;  Bit 45-46 */
		USHORT P : 1;    /* 7;  Bit 47 */

		USHORT AVL : 1; /* 8;  Bit 52 */
		USHORT L : 1;   /* 9;  Bit 53 */
		USHORT DB : 1;  /* 10; Bit 54 */
		USHORT G : 1;   /* 11; Bit 55 */
		USHORT GAP : 4;

	} Fields;
} SEGMENT_ATTRIBUTES;

typedef struct SEGMENT_SELECTOR
{
	USHORT             SEL;
	SEGMENT_ATTRIBUTES ATTRIBUTES;
	ULONG32            LIMIT;
	ULONG64            BASE;
} SEGMENT_SELECTOR, * PSEGMENT_SELECTOR;


typedef struct _SEGMENT_DESCRIPTOR {
	USHORT LIMIT0;
	USHORT BASE0;
	UCHAR  BASE1;
	UCHAR  ATTR0;
	UCHAR  LIMIT1ATTR1;
	UCHAR  BASE2;
} SEGMENT_DESCRIPTOR, * PSEGMENT_DESCRIPTOR;

enum SEGREGS
{
	ES = 0,
	CS,
	SS,
	DS,
	FS,
	GS,
	LDTR,
	TR
};

typedef union _MSR
{
	struct
	{
		ULONG Low;
		ULONG High;
	};

	ULONG64 Content;
} MSR, * PMSR;

typedef union _IA32_MTRR_FIXED_RANGE_TYPE
{
	UINT64 All;
	struct
	{
		UINT8 Types[8];
	} s;
} IA32_MTRR_FIXED_RANGE_TYPE;

constexpr SIZE_T MAX_VARIABLE_RANGE_MTRRS = 255;
constexpr SIZE_T FIXED_RANGE_MTRRS_SIZE = ((1 + 2 + 8) * RTL_NUMBER_OF_FIELD(IA32_MTRR_FIXED_RANGE_TYPE, s.Types));
constexpr SIZE_T MTRR_ENTRIES_SIZE = MAX_VARIABLE_RANGE_MTRRS + FIXED_RANGE_MTRRS_SIZE;

typedef union _MOV_CR_QUALIFICATION
{
	ULONG_PTR All;
	struct
	{
		ULONG ControlRegister : 4;
		ULONG AccessType : 2;
		ULONG LMSWOperandType : 1;
		ULONG Reserved1 : 1;
		ULONG Register : 4;
		ULONG Reserved2 : 4;
		ULONG LMSWSourceData : 16;
		ULONG Reserved3;
	} Fields;
} MOV_CR_QUALIFICATION, * PMOV_CR_QUALIFICATION;

typedef struct _EPT_HOOKED_PAGE_DETAIL {
	LIST_ENTRY Entry;
	UINT64 VirtualAddress;
	UINT64 PhysicalAddress;
	UINT64 PhysicalBaseAddress;
	PEPT_PML1_ENTRY EntryAddress;
	EPT_PML1_ENTRY OriginalEntry;
	EPT_PML1_ENTRY ChangedEntry;
	PCHAR InlineHook;
	bool IsExecutionHook;
} EPT_HOOKED_PAGE_DETAIL, * PEPT_HOOKED_PAGE_DETAIL;

typedef struct {
	UINT8 Permissions;
	UINT64 Address;
} HookedPage;

#pragma warning(pop)