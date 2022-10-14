#include "pch.h"
#include "DbgDriver.h"
#include <winternl.h>
#include "resource.h"

#define IOCTL_DEBUG_CONTROL \
    CTL_CODE(FILE_DEVICE_UNKNOWN, 1, METHOD_NEITHER, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

static constexpr WCHAR kernelDbgDriverName[] = L"kldbgdrv";

enum class SysDbgCommand : ULONG {
    QueryModuleInformation = 0,
    QueryTraceInformation = 1,
    QueryVersion = 7,
    ReadVirtual = 8,
    WriteVirtual = 9,
    ReadPhysical = 10,
    WritePhysical = 11,
    ReadControlSpace = 12,
    WriteControlSpace = 13,
    ReadIoSpace = 14,
    WriteIoSpace = 15,
    ReadMsr = 16,
    WriteMsr = 17,
    ReadBusData = 18,
    WriteBusData = 19,
    CheckLowMemory = 20,
    EnableKernelDebugger = 21,
    DisableKernelDebugger = 22,
    GetAutoKdEnable = 23,
    SetAutoKdEnable = 24,
    GetPrintBufferSize = 25,
    SetPrintBufferSize = 26,
    GetTriageDump = 29,
    GetKdBlockEnable = 30,
    SetKdBlockEnable = 31,
    RegisterForUmBreakInfo = 32,
    GetLiveKernelDump = 37,
};

struct DriverDebugControl {
    SysDbgCommand Command;
    PVOID InputBuffer;
    ULONG InputBufferLength;
};

struct SysDbgVirtual {
    PVOID Address;
    PVOID Buffer;
    ULONG Request;
};

ULONG ReadWriteCommon(HANDLE hDevice, SysDbgCommand cmd, PVOID address, ULONG size, PVOID buffer) {
    DriverDebugControl data;
    data.Command = cmd;
    SysDbgVirtual dbg;
    dbg.Address = address;
    dbg.Request = size;
    dbg.Buffer = buffer;
    data.InputBuffer = &dbg;
    data.InputBufferLength = sizeof(dbg);
    DWORD ret;
    return ::DeviceIoControl(hDevice, IOCTL_DEBUG_CONTROL, &data, sizeof(data), &dbg, sizeof(dbg), &ret, nullptr) ? ret : 0;
}

DbgDriver& DbgDriver::Get() {
    static DbgDriver driver;
    return driver;
}

DbgDriver::~DbgDriver() {
    Close();
}

DbgDriver::operator bool() const {
    return m_hDevice != nullptr;
}

bool DbgDriver::Open() {
    if (m_hDevice)
        return true;

    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, L"\\Device\\kldbgdrv");
    OBJECT_ATTRIBUTES attr;
    InitializeObjectAttributes(&attr, &devName, OBJ_CASE_INSENSITIVE, nullptr, nullptr);
    IO_STATUS_BLOCK ioStatus;
    auto status = NtOpenFile(&m_hDevice, GENERIC_READ | GENERIC_WRITE, &attr, &ioStatus, 0, FILE_OPEN);
    if (status == 0xc0000034 /* Object name not found */ && Install())
        return Open();
    return NT_SUCCESS(status);
}

void DbgDriver::Close() {
    if (m_hDevice) {
        ::CloseHandle(m_hDevice);
        m_hDevice = nullptr;
    }
}

bool DbgDriver::Install() {
    auto hScm = ::OpenSCManager(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!hScm)
        return false;

    auto hService = ::OpenService(hScm, kernelDbgDriverName, SERVICE_START);
    if (!hService) {
        WCHAR path[MAX_PATH];
        ::GetSystemDirectory(path, _countof(path));
        wcscat_s(path, L"\\Drivers\\kldbgdrv.sys");
        if (!WriteFileFromResource(path, IDR_DRIVER))
            return false;

        hService = ::CreateServiceW(hScm, kernelDbgDriverName, nullptr, 
            SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL, path, nullptr, nullptr, nullptr, nullptr, nullptr);
    }
    ::CloseServiceHandle(hScm);
    if (!hService)
        return false;

    auto ok = ::StartService(hService, 0, nullptr);
    ::CloseServiceHandle(hService);
    return ok;
}

ULONG DbgDriver::ReadVirtual(PVOID address, ULONG size, PVOID buffer) {
    return ReadWriteCommon(m_hDevice, SysDbgCommand::ReadVirtual, address, size, buffer);
}

ULONG DbgDriver::WriteVirtual(PVOID address, ULONG size, PVOID buffer) {
    return ReadWriteCommon(m_hDevice, SysDbgCommand::WriteVirtual, address, size, buffer);
}

bool DbgDriver::WriteFileFromResource(PCWSTR path, UINT id, PCWSTR type) {
    auto hModule = ::GetModuleHandle(nullptr);
    auto hResource = ::FindResource(hModule, MAKEINTRESOURCE(id), type);
    if (!hResource)
        return false;

    auto size = ::SizeofResource(hModule, hResource);
    auto hGlobal = ::LoadResource(hModule, hResource);
    if (!hGlobal || size == 0)
        return false;
    auto p = ::LockResource(hGlobal);
    auto hFile = ::CreateFile(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return false;

    DWORD written;
    auto ok = ::WriteFile(hFile, p, size, &written, nullptr);
    ::CloseHandle(hFile);
    return ok;
}

