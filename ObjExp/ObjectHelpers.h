#pragma once

#include <map>

struct ObjectHelpers abstract final {
	static UINT ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name = nullptr, PCWSTR target = nullptr, DWORD handleCount = 0, DWORD pid = 0);
	static std::vector<std::pair<CString, CString>> GetSimpleProps(HANDLE hObject, PCWSTR type, PCWSTR name, PCWSTR target = nullptr);
	static bool IsNamedObjectType(USHORT index);
	static HANDLE OpenObject(PCWSTR path, PCWSTR typeName, DWORD access);
	static PVOID GetObjectAddress(PCWSTR fullName);
	// objects in the object manager namespace (by full name); failures are reported to the user
	static bool ShowNamespaceObjectProperties(HWND hParent, PCWSTR fullName, PCWSTR type, PCWSTR target = nullptr);
	static bool EditNamespaceObjectSecurity(HWND hParent, PCWSTR fullName, PCWSTR type);

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
		{ L"Timer", L"_ETIMER" },
		{ L"IoCompletion", L"_KQUEUE" },
		{ L"IoCompletionReserve", L"_IO_MINI_COMPLETION_PACKET_USER" },
		{ L"EtwRegistration", L"_ETW_REG_ENTRY" },
		{ L"EtwConsumer", L"_ETW_REALTIME_CONSUMER" },
		{ L"TmTm", L"_KTM" },
		{ L"TmTx", L"_KTRANSACTION" },
		{ L"TmRm", L"_KRESOURCEMANAGER" },
		{ L"TmEn", L"_KENLISTMENT" },
		{ L"Controller", L"_CONTROLLER_OBJECT" },
		{ L"Partition", L"_EPARTITION" },
		//
		// not in the public symbols: Desktop and WindowStation (win32k's tagDESKTOP / tagWINDOWSTATION),
		// TpWorkerFactory, Callback, DebugObject, WmiGuid, WaitCompletionPacket, KeyedEvent
		//
	};
};

