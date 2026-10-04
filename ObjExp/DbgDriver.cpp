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
    auto open = [&] {
        return NtOpenFile(&m_hDevice, GENERIC_READ | GENERIC_WRITE, &attr, &ioStatus, 0, FILE_OPEN);
    };
    auto status = open();
    if (status == 0xc0000034 /* Object name not found */ && Install())
        status = open();
    if (!NT_SUCCESS(status)) {
        m_hDevice = nullptr;
        Uninstall();
        return false;
    }
    return true;
}

void DbgDriver::Close() {
    // the driver stays loaded for other instances and later runs
    if (m_hDevice) {
        ::CloseHandle(m_hDevice);
        m_hDevice = nullptr;
    }
}

bool DbgDriver::Install() {
    wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
    if (!hScm)
        return false;

    WCHAR path[MAX_PATH];
    ::GetSystemDirectory(path, _countof(path));
    wcscat_s(path, L"\\Drivers\\kldbgdrv.sys");

    //
    // the file may be missing even if the service exists
    //
    if (::GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) {
        if (!WriteFileFromResource(path, IDR_DRIVER))
            return false;
        m_DriverPath = path;
        m_WroteFile = true;
    }

    const DWORD access = SERVICE_START | SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE;
    wil::unique_schandle hService(::OpenService(hScm.get(), kernelDbgDriverName, access));
    if (!hService) {
        hService.reset(::CreateService(hScm.get(), kernelDbgDriverName, nullptr,
            access, SERVICE_KERNEL_DRIVER, SERVICE_DEMAND_START,
            SERVICE_ERROR_NORMAL, path, nullptr, nullptr, nullptr, nullptr, nullptr));
        if (!hService) {
            Uninstall();
            return false;
        }
        m_CreatedService = true;
    }

    if (::StartService(hService.get(), 0, nullptr)) {
        m_Started = true;
        return true;
    }
    if (::GetLastError() == ERROR_SERVICE_ALREADY_RUNNING)
        return true;

    Uninstall();
    return false;
}

void DbgDriver::Uninstall() {
    if (m_Started || m_CreatedService) {
        wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT));
        wil::unique_schandle hService(hScm ? ::OpenService(hScm.get(), kernelDbgDriverName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE) : nullptr);
        if (hService) {
            if (m_Started) {
                SERVICE_STATUS status;
                ::ControlService(hService.get(), SERVICE_CONTROL_STOP, &status);
            }
            if (m_CreatedService)
                ::DeleteService(hService.get());
        }
    }
    if (m_WroteFile) {
        //
        // the driver image may still be in use for a moment after the driver stops
        //
        for (int i = 0; i < 10 && !::DeleteFile(m_DriverPath.c_str()); i++) {
            auto error = ::GetLastError();
            if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION)
                break;
            ::Sleep(100);
        }
    }
    m_Started = m_CreatedService = m_WroteFile = false;
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

