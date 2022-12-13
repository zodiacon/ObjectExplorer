#pragma once

#include <map>

struct ObjectHelpers abstract final {
	static UINT ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name = nullptr, PCWSTR target = nullptr, DWORD handleCount = 0);
	static std::vector<std::pair<CString, CString>> GetSimpleProps(HANDLE hObject, PCWSTR type, PCWSTR name, PCWSTR target = nullptr);
	static bool IsNamedObjectType(USHORT index);
	static HANDLE OpenObject(PCWSTR path, PCWSTR typeName, DWORD access);
	static PVOID GetObjectAddress(PCWSTR fullName);

	inline static std::map<CString, CString> KernelTypes{
		{ L"ALPC Port", L"_ALPC_PORT" },
		{ L"Process", L"_EPROCESS" },
		{ L"Semaphore", L"_KSEMAPHORE" },
		{ L"Job", L"_EJOB" },
		{ L"Mutant", L"_KMUTANT" },
		{ L"Event", L"_KEVENT" },
		{ L"Thread", L"_ETHREAD" },
		{ L"Section", L"_SECTION" },
		{ L"File", L"_FILE_OBJECT" },
		{ L"Type", L"_OBJECT_TYPE" },
		{ L"Key", L"_CM_KEY_BODY" },
		{ L"Token", L"_TOKEN" },
		{ L"SymbolicLink", L"_OBJECT_SYMBOLIC_LINK" },
		{ L"Driver", L"_DRIVER_OBJECT" },
		{ L"Device", L"_DEVICE_OBJECT" },
		{ L"Directory", L"_OBJECT_DIRECTORY" },
	};
};

