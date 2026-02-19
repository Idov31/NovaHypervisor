# NovaHypervisor

<p align="center">
  <img alt="Logo" src="./Images/logo_transparent.png" width="400" height="400">
</p>

![image](https://img.shields.io/badge/C%2B%2B-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white) ![assembly](https://img.shields.io/badge/ASSEMBLY-ED8B00?style=for-the-badge&logo=Assembly&logoColor=white) ![image](https://img.shields.io/badge/Windows-0078D6?style=for-the-badge&logo=windows&logoColor=white)

## Description

NovaHypervisor is a defensive x64 Intel host based hypervisor. The goal of this project is to protect against kernel based attacks (either via Bring Your Own Vulnerable Driver (BYOVD) or other means) by safeguarding defense products (AntiVirus / Endpoint Protection) and kernel memory structures and preventing unauthorized access to kernel memory.

NovaHypervisor is written in C++ and Assembly, and is designed to be compatible to run on Windows 10 and later versions. Please see the [setup](#setup) section for more information on how to use it.

> [!IMPORTANT]  
> The hypervisor was tested on Windows 11 25H2 and multiple Windows 10 versions.
> If you encounter a problem, please open an issue after checking there isn't already an open issue.

## Supported Hypervisors

NovaHypervisor can run under these hypervisors:

### Legend

✅ - Supported and tested

⌛ - Work in progress

❌ - Not supported and not planned to be supported

| Hypervisor | Supported |
|------------|-----------|
| VMware     | ✅        |
| Hyper-V    | ✅        |
| Hyper-V with VBS | ⌛  |
| VirtualBox | ❌        |
| QEMU       | ❌        |
| KVM        | ❌        |

## Usage

To use the NovaHypervisor, you will need to create a kernel service and start it:

```cmd
sc create NovaHypervisor type= kernel binPath= "C:\Path\To\NovaHypervisor.sys"

sc start NovaHypervisor
```

Then, you can add and remove the addresses that you want to protect using the [NovaClient](./NovaClient/) application:

```cmd
REM Add an address to protect
NovaClient.exe protect 0x12345678 <r|w|x> <execution hook>

REM Remove an address from protection
NovaClient.exe unprotect 0x12345678
```

- protect: Protect a memory address from being accessed, you can specify the type of protection:
  - `r`: Read protection
  - `w`: Write protection
  - `x`: Execute protection
The protection that you give is the protection that the address will **have**. For example, if you want to remove execute privileges, do "rw".

- unprotect: Remove protection from a memory address.

> [!NOTE]
> Execution hook via inline hook + EPT hooks are not supported and will not be supported for this project to prevent abuse.

## Setup

### Compiling the Project

The setup to compile the project requires you to have:

- Visual Studio 2022 or later.
- Windows Driver Kit (WDK) installed.

### Target setup

To run the hypervisor, you will need to have a Windows 10 or later version installed on your machine. You will also need to have:

- Intel VT-x enabled.
- Virtualized IOMMU.

## Logging and Debugging

### Logging

NovaHypervisor uses [WPP](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/wpp-software-tracing) logging as it provides easy to use interface that works also in VMX root. To be able to see the logs, make sure to create a trace session once:

```cmd
logman create trace "NovaHypervisorLogs" -p {e74c1035-77d4-4c5b-9088-77056fae3aa3} 0xffffffff 0xff -o C:\Path\To\NovaHypervisor.etl
```

Later on, whenever you want to start or end the logging session you can use:

```cmd
logman start "NovaHypervisorLogs"
logman stop "NovaHypervisorLogs"
```

To view the logs you can use tools such as [TraceView](https://learn.microsoft.com/en-us/windows-hardware/drivers/devtest/traceview).

### Debugging

To test and debug it in your testing environment run those commands with elevated cmd and then restart your machine:

```cmd
bcdedit /set testsigning on
bcdedit /debug on
bcdedit /dbgsettings net hostip:<HOSTIP> port:55000 key:1.2.3.4
```

Where `<HOSTIP>` is the IP address of your host machine.

## Resources

- [Hypervisor From Scratch](https://rayanfam.com/topics/hypervisor-from-scratch-part-1/)

- [HyperDbg](https://github.com/HyperDbg/HyperDbg)

- [QEMU](https://www.qemu.org/)

- [TLFS](https://raw.githubusercontent.com/Microsoft/Virtualization-Documentation/master/tlfs/Hypervisor%20Top%20Level%20Functional%20Specification%20v5.0.pdf)

## Personal Thanks & Contributors

- [Sinaei](https://x.com/Intel80x86): For his help with answering questions I had and for his amazing work on HyperDbg and Hypervisor From Scratch.

- [memN0ps](https://github.com/memN0ps/): For his help with answering questions I had and pointing me to the right resources.
