#include "pch.h"
#include "resource.h"
#include "DriverHelper.h"
#include "SecurityHelper.h"

#define DRIVER_CURRENT_VERSION 0x0104

#define IOCTL_KOBJEXP_OPEN_OBJECT				CTL_CODE(0x8000, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_DUP_HANDLE				CTL_CODE(0x8000, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_EVENT_BY_NAME		CTL_CODE(0x8000, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_SEMAPHORE_BY_NAME	CTL_CODE(0x8000, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_JOB_BY_NAME			CTL_CODE(0x8000, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_DESKTOP_BY_NAME		CTL_CODE(0x8000, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_PROCESS				CTL_CODE(0x8000, 0x806, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_GET_VERSION				CTL_CODE(0x8000, 0x807, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_GET_OBJECT_ADDRESS		CTL_CODE(0x8000, 0x808, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_KOBJEXP_OPEN_THREAD				CTL_CODE(0x8000, 0x809, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct OpenObjectData {
	void* Address;
	ACCESS_MASK Access;
};

struct DupHandleData {
	ULONG Handle;
	ULONG SourcePid;
	ACCESS_MASK AccessMask;
	ULONG Flags;
};

struct OpenProcessThreadData {
	ULONG Id;
	ACCESS_MASK AccessMask;
};

HANDLE DriverHelper::OpenHandle(void* pObject, ACCESS_MASK access) {
	if (!OpenDevice())
		return nullptr;

	OpenObjectData data;
	data.Access = access;
	data.Address = pObject;

	DWORD bytes;
	HANDLE hObject;
	return ::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_OPEN_OBJECT, &data, sizeof(data),
		&hObject, sizeof(hObject), &bytes, nullptr)
		? hObject : nullptr;
}

HANDLE DriverHelper::DupHandle(HANDLE hObject, ULONG pid, ACCESS_MASK access, DWORD flags) {
	HANDLE hTarget = nullptr;
	if (OpenDevice()) {
		DupHandleData data;
		data.AccessMask = access;
		data.Handle = HandleToUlong(hObject);
		data.SourcePid = pid;
		data.Flags = flags;

		DWORD bytes;
		::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_DUP_HANDLE, &data, sizeof(data),
			&hTarget, sizeof(hTarget), &bytes, nullptr);
	}
	if (!hTarget) {
		wil::unique_handle hProcess(OpenProcess(pid, PROCESS_DUP_HANDLE));
		if (hProcess)
			::DuplicateHandle(hProcess.get(), hObject, ::GetCurrentProcess(), &hTarget, access, FALSE, flags);
	}
	return hTarget;
}

HANDLE DriverHelper::OpenProcess(DWORD pid, ACCESS_MASK access) {
	if (OpenDevice()) {
		OpenProcessThreadData data;
		data.AccessMask = access;
		data.Id = pid;
		HANDLE hProcess;
		DWORD bytes;

		return ::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_OPEN_PROCESS, &data, sizeof(data),
			&hProcess, sizeof(hProcess), &bytes, nullptr) ? hProcess : nullptr;
	}
	return ::OpenProcess(access, FALSE, pid);
}

HANDLE DriverHelper::OpenThread(DWORD tid, ACCESS_MASK access) {
	if (OpenDevice()) {
		OpenProcessThreadData data;
		data.AccessMask = access;
		data.Id = tid;
		HANDLE hThread;
		DWORD bytes;

		return ::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_OPEN_THREAD, &data, sizeof(data),
			&hThread, sizeof(hThread), &bytes, nullptr) ? hThread : nullptr;
	}
	return ::OpenThread(access, FALSE, tid);
}

PVOID DriverHelper::GetObjectAddress(HANDLE hObject) {
	if (!OpenDevice())
		return nullptr;

	PVOID address = nullptr;
	DWORD bytes;
	::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_GET_OBJECT_ADDRESS, &hObject, sizeof(hObject), &address, sizeof(address), &bytes, nullptr);
	return address;
}

USHORT DriverHelper::GetVersion() {
	USHORT version = 0;
	if (!OpenDevice())
		return 0;

	DWORD bytes;
	::DeviceIoControl(s_hDevice, IOCTL_KOBJEXP_GET_VERSION, nullptr, 0, &version, sizeof(version), &bytes, nullptr);
	return version;
}

USHORT DriverHelper::GetCurrentVersion() {
	return DRIVER_CURRENT_VERSION;
}

bool DriverHelper::CloseDevice() {
	if (s_hDevice) {
		::CloseHandle(s_hDevice);
		s_hDevice = nullptr;
	}
	return true;
}

bool DriverHelper::IsDriverLoaded() {
	wil::unique_schandle hScm(::OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE));
	if (!hScm)
		return false;

	wil::unique_schandle hService(::OpenService(hScm.get(), L"KObjExp", SERVICE_QUERY_STATUS));
	if (!hService)
		return false;

	SERVICE_STATUS status;
	if (!::QueryServiceStatus(hService.get(), &status))
		return false;

	return status.dwCurrentState == SERVICE_RUNNING;
}

bool DriverHelper::OpenDevice() {
	if (!s_hDevice) {
		s_hDevice = ::CreateFile(L"\\\\.\\KObjExp", GENERIC_WRITE | GENERIC_READ,
			FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
			OPEN_EXISTING, 0, nullptr);
		if (s_hDevice == INVALID_HANDLE_VALUE) {
			s_hDevice = nullptr;
			return false;
		}
	}
	return true;
}
