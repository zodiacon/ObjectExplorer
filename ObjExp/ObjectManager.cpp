#include "pch.h"
#include "ObjectManager.h"
#include "DriverHelper.h"

int ObjectManager::EnumTypes() {
	const ULONG len = 1 << 14;
	wil::unique_virtualalloc_ptr<> buffer(::VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (!NT_SUCCESS(NT::NtQueryObject(nullptr, NT::ObjectTypesInformation, buffer.get(), len, nullptr)))
		return 0;

	auto p = static_cast<NT::OBJECT_TYPES_INFORMATION*>(buffer.get());
	bool empty = s_types.empty();

	auto count = p->NumberOfTypes;
	if (empty) {
		s_types.reserve(count);
		s_typesMap.reserve(count);
		s_changes.reserve(32);
	}
	else {
		s_changes.clear();
		ATLASSERT(count == s_types.size());
	}
	auto raw = &p->TypeInformation[0];
	TotalHandles = TotalObjects = PeakObjects = PeakHandles = 0;

	for (ULONG i = 0; i < count; i++) {
		if (!::IsWindows8OrGreater()) {
			// TypeIndex is only supported since Win8. Uses the fake index for previous OS.
			raw->TypeIndex = static_cast<decltype(raw->TypeIndex)>(i);
		}
		auto type = empty ? std::make_shared<ObjectTypeInfo>() : s_typesMap[raw->TypeIndex];
		if (empty) {
			type->GenericMapping = raw->GenericMapping;
			type->TypeIndex = raw->TypeIndex;
			type->DefaultNonPagedPoolCharge = raw->DefaultNonPagedPoolCharge;
			type->DefaultPagedPoolCharge = raw->DefaultPagedPoolCharge;
			type->TypeName = CString(raw->TypeName.Buffer, raw->TypeName.Length / sizeof(WCHAR));
			type->PoolType = (PoolType)raw->PoolType;
			type->DefaultNonPagedPoolCharge = raw->DefaultNonPagedPoolCharge;
			type->DefaultPagedPoolCharge = raw->DefaultPagedPoolCharge;
			type->ValidAccessMask = raw->ValidAccessMask;
			type->InvalidAttributes = raw->InvalidAttributes;
		}
		else {
			if (type->TotalNumberOfHandles != raw->TotalNumberOfHandles)
				s_changes.push_back({ type, ChangeType::TotalHandles, (int32_t)raw->TotalNumberOfHandles - (int32_t)type->TotalNumberOfHandles });
			if (type->TotalNumberOfObjects != raw->TotalNumberOfObjects)
				s_changes.push_back({ type, ChangeType::TotalObjects, (int32_t)raw->TotalNumberOfObjects - (int32_t)type->TotalNumberOfObjects });
			if (type->HighWaterNumberOfHandles != raw->HighWaterNumberOfHandles)
				s_changes.push_back({ type, ChangeType::PeakHandles, (int32_t)raw->HighWaterNumberOfHandles - (int32_t)type->HighWaterNumberOfHandles });
			if (type->HighWaterNumberOfObjects != raw->HighWaterNumberOfObjects)
				s_changes.push_back({ type, ChangeType::PeakObjects, (int32_t)raw->HighWaterNumberOfObjects - (int32_t)type->HighWaterNumberOfObjects });
		}

		type->TotalNumberOfHandles = raw->TotalNumberOfHandles;
		type->TotalNumberOfObjects = raw->TotalNumberOfObjects;
		type->TotalNonPagedPoolUsage = raw->TotalNonPagedPoolUsage;
		type->TotalPagedPoolUsage = raw->TotalPagedPoolUsage;
		type->HighWaterNumberOfObjects = raw->HighWaterNumberOfObjects;
		type->HighWaterNumberOfHandles = raw->HighWaterNumberOfHandles;
		type->TotalNamePoolUsage = raw->TotalNamePoolUsage;

		TotalObjects += raw->TotalNumberOfObjects;
		TotalHandles += raw->TotalNumberOfHandles;
		PeakObjects += raw->HighWaterNumberOfObjects;
		PeakHandles += raw->HighWaterNumberOfHandles;

		if (empty) {
			s_types.emplace_back(type);
			s_typesMap.insert({ type->TypeIndex, type });
			s_typesNameMap.insert({ std::wstring(type->TypeName), type });
		}

		auto temp = (BYTE*)raw + sizeof(NT::OBJECT_TYPE_INFORMATION) + raw->TypeName.MaximumLength;
		temp += sizeof(PVOID) - 1;
		raw = reinterpret_cast<NT::OBJECT_TYPE_INFORMATION*>((ULONG_PTR)temp / sizeof(PVOID) * sizeof(PVOID));
	}
	return static_cast<int>(s_types.size());
}

const std::vector<ObjectManager::Change>& ObjectManager::GetChanges() {
	return s_changes;
}

std::shared_ptr<ObjectTypeInfo> ObjectManager::GetType(PCWSTR name) {
	if (s_types.empty())
		EnumTypes();

	return s_typesNameMap.at(name);
}

const std::vector<std::shared_ptr<ObjectTypeInfo>>& ObjectManager::GetObjectTypes() {
	return s_types;
}

const std::vector<std::shared_ptr<HandleInfo>>& ObjectManager::GetHandles() const {
	return m_handles;
}

std::vector<ObjectNameAndType> ObjectManager::EnumDirectoryObjects(PCWSTR path) {
	std::vector<ObjectNameAndType> objects;
	wil::unique_handle hDirectory;
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, path);
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (!NT_SUCCESS(NT::NtOpenDirectoryObject(hDirectory.addressof(), DIRECTORY_QUERY, &attr)))
		return objects;

	objects.reserve(128);
	BYTE buffer[1 << 12];
	auto info = reinterpret_cast<NT::OBJECT_DIRECTORY_INFORMATION*>(buffer);
	bool first = true;
	ULONG size, index = 0;
	for (;;) {
		auto start = index;
		if (!NT_SUCCESS(NT::NtQueryDirectoryObject(hDirectory.get(), info, sizeof(buffer), FALSE, first, &index, &size)))
			break;
		first = false;
		for (ULONG i = 0; i < index - start; i++) {
			ObjectNameAndType data;
			auto& p = info[i];
			data.Name = std::wstring(p.Name.Buffer, p.Name.Length / sizeof(WCHAR));
			data.TypeName = std::wstring(p.TypeName.Buffer, p.TypeName.Length / sizeof(WCHAR));

			objects.push_back(std::move(data));
		}
	}
	return objects;

}

CString ObjectManager::GetSymbolicLinkTarget(PCWSTR path) {
	wil::unique_handle hLink;
	OBJECT_ATTRIBUTES attr;
	CString target;
	UNICODE_STRING name;
	RtlInitUnicodeString(&name, path);
	InitializeObjectAttributes(&attr, &name, 0, nullptr, nullptr);
	if (NT_SUCCESS(NT::NtOpenSymbolicLinkObject(hLink.addressof(), GENERIC_READ, &attr))) {
		WCHAR buffer[1 << 10];
		UNICODE_STRING result;
		result.Buffer = buffer;
		result.MaximumLength = sizeof(buffer);
		if (NT_SUCCESS(NT::NtQuerySymbolicLinkObject(hLink.get(), &result, nullptr)))
			target.SetString(result.Buffer, result.Length / sizeof(WCHAR));
	}
	return target;
}

wil::unique_virtualalloc_ptr<NT::SYSTEM_HANDLE_INFORMATION_EX> ObjectManager::EnumHandlesBuffer() {
	ULONG len = 1 << 25;
	wil::unique_virtualalloc_ptr<NT::SYSTEM_HANDLE_INFORMATION_EX> buffer;
	do {
		buffer.reset((NT::SYSTEM_HANDLE_INFORMATION_EX*)::VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		auto status = NT::NtQuerySystemInformation(NT::SystemExtendedHandleInformation, buffer.get(), len, &len);
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			len <<= 1;
			continue;
		}
		if (status == STATUS_SUCCESS)
			break;
		return {};
	} while (true);

	return buffer;
}

HANDLE ObjectManager::DupHandle(HANDLE h, DWORD pid, ACCESS_MASK access, DWORD flags) {
	HANDLE hDup{ nullptr };
	wil::unique_handle hProcess(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, pid));
	if (hProcess)
		::DuplicateHandle(hProcess.get(), h, ::GetCurrentProcess(), &hDup, access, FALSE, flags);
	if (hDup)
		return hDup;

	return DriverHelper::DupHandle(h, pid, access, flags);
}

NTSTATUS ObjectManager::OpenObject(PCWSTR path, PCWSTR typeName, HANDLE& hObject, DWORD access) {
	hObject = nullptr;
	CString type(typeName);
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING uname;
	RtlInitUnicodeString(&uname, path);
	InitializeObjectAttributes(&attr, &uname, 0, nullptr, nullptr);
	NTSTATUS status = STATUS_UNSUCCESSFUL;

	if (type == L"Event")
		status = NT::NtOpenEvent(&hObject, access, &attr);
	else if (type == L"Mutant")
		status = NT::NtOpenMutant(&hObject, access, &attr);
	else if (type == L"ALPC Port") {
		//
		// special case: find a handle to this named object and duplicate the handle
		//
		auto [handle, pid] = FindFirstHandle(path, GetType(typeName)->TypeIndex);
		if(handle)
			hObject = DriverHelper::DupHandle(handle, pid, access, 0);
	}
	else if (type == L"Section")
		status = NT::NtOpenSection(&hObject, access, &attr);
	else if (type == L"Semaphore")
		status = NT::NtOpenSemaphore(&hObject, access, &attr);
	else if (type == "EventPair")
		status = NT::NtOpenEventPair(&hObject, access, &attr);
	else if (type == L"IoCompletion")
		status = NT::NtOpenIoCompletion(&hObject, access, &attr);
	else if (type == L"SymbolicLink")
		status = NT::NtOpenSymbolicLinkObject(&hObject, access, &attr);
	else if (type == L"Key")
		status = NT::NtOpenKey(&hObject, access, &attr);
	else if (type == L"Job")
		status = NT::NtOpenJobObject(&hObject, access, &attr);
	else if (type == L"Session")
		status = NT::NtOpenSession(&hObject, access, &attr);
	else if (type == L"WindowStation") {
		hObject = NT::NtUserOpenWindowStation(&attr, access);
		status = hObject ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
	}
	else if (type == L"Directory")
		status = NT::NtOpenDirectoryObject(&hObject, access, &attr);
	else if (type == L"File" || type == L"Device") {
		IO_STATUS_BLOCK ioStatus;
		status = NT::NtOpenFile(&hObject, access, &attr, &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
	}

	return status;
}

std::pair<HANDLE, DWORD> ObjectManager::FindFirstHandle(PCWSTR name, USHORT index, DWORD pid) {
	ULONG len = 1 << 25;
	wil::unique_virtualalloc_ptr<> buffer;
	do {
		buffer.reset(::VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		auto status = NT::NtQuerySystemInformation(NT::SystemExtendedHandleInformation, buffer.get(), len, &len);
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			len <<= 1;
			continue;
		}
		if (status == 0)
			break;
		return {};
	} while (true);

	auto p = (NT::SYSTEM_HANDLE_INFORMATION_EX*)buffer.get();
	auto count = p->NumberOfHandles;
	for (decltype(count) i = 0; i < count; i++) {
		auto& handle = p->Handles[i];
		if (pid && handle.UniqueProcessId != pid)
			continue;

		if (index && handle.ObjectTypeIndex != index)
			continue;

		CString oname = GetObjectName((HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId, handle.ObjectTypeIndex);
		if (oname == name)
			return { (HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId };
	}

	return {};
}

bool ObjectManager::EnumHandles(PCWSTR type, DWORD pid, bool namedObjectsOnly) {
	EnumTypes();

	ULONG len = 1 << 25;
	wil::unique_virtualalloc_ptr<> buffer;
	do {
		buffer.reset(::VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		auto status = NT::NtQuerySystemInformation(NT::SystemExtendedHandleInformation, buffer.get(), len, &len);
		if (status == STATUS_INFO_LENGTH_MISMATCH) {
			len <<= 1;
			continue;
		}
		if (status == 0)
			break;
		return false;
	} while (true);

	auto filteredTypeIndex = type == nullptr || ::wcslen(type) == 0 ? -1 : s_typesNameMap.at(type)->TypeIndex;

	auto p = (NT::SYSTEM_HANDLE_INFORMATION_EX*)buffer.get();
	auto count = p->NumberOfHandles;
	m_handles.clear();
	m_handles.reserve(count);
	for (decltype(count) i = 0; i < count; i++) {
		auto& handle = p->Handles[i];
		if (pid && handle.UniqueProcessId != pid)
			continue;

		if (filteredTypeIndex >= 0 && handle.ObjectTypeIndex != filteredTypeIndex)
			continue;

		// skip current process
		if (m_skipThisProcess && handle.UniqueProcessId == ::GetCurrentProcessId())
			continue;

		CString name;
		if (namedObjectsOnly && (name = GetObjectName((HANDLE)handle.HandleValue, (DWORD)handle.UniqueProcessId, handle.ObjectTypeIndex)).IsEmpty())
			continue;

		auto hi = std::make_shared<HandleInfo>();
		hi->HandleValue = (ULONG)handle.HandleValue;
		hi->GrantedAccess = handle.GrantedAccess;
		hi->Object = handle.Object;
		hi->HandleAttributes = handle.HandleAttributes;
		hi->ProcessId = (ULONG)handle.UniqueProcessId;
		hi->ObjectTypeIndex = handle.ObjectTypeIndex;
		hi->Name = name;

		m_handles.emplace_back(std::move(hi));
	}

	return true;
}

CString ObjectManager::GetObjectName(HANDLE hDup, USHORT type) {
	ATLASSERT(!s_types.empty());
	static int processTypeIndex = s_typesNameMap.at(L"Process")->TypeIndex;
	static int threadTypeIndex = s_typesNameMap.at(L"Thread")->TypeIndex;
	static int fileTypeIndex = s_typesNameMap.at(L"File")->TypeIndex;
	ATLASSERT(processTypeIndex > 0 && threadTypeIndex > 0);

	CString sname;

	do {
		if (type == processTypeIndex || type == threadTypeIndex)
			break;

		BYTE buffer[2048];
		if (type == fileTypeIndex) {
			// special case for files in case they're locked
			struct Data {
				HANDLE hDup;
				BYTE* buffer;
			} data = { hDup, buffer };

			wil::unique_handle hThread(::CreateThread(nullptr, 1 << 13, [](auto p) {
				auto d = (Data*)p;
				return (DWORD)NT::NtQueryObject(d->hDup, NT::ObjectNameInformation, d->buffer, sizeof(buffer), nullptr);
				}, &data, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr));
			if (::WaitForSingleObject(hThread.get(), 10) == WAIT_TIMEOUT) {
				::TerminateThread(hThread.get(), 1);
			}
			else {
				DWORD code;
				::GetExitCodeThread(hThread.get(), &code);
				if (code == STATUS_SUCCESS) {
					auto name = (NT::POBJECT_NAME_INFORMATION)buffer;
					sname = CString(name->Name.Buffer, name->Name.Length / sizeof(WCHAR));
				}
			}
		}
		else if (NT_SUCCESS(NT::NtQueryObject(hDup, NT::ObjectNameInformation, buffer, sizeof(buffer), nullptr))) {
			auto name = (NT::POBJECT_NAME_INFORMATION)buffer;
			sname = CString(name->Name.Buffer, name->Name.Length / sizeof(WCHAR));
		}
	} while (false);

	return sname;
}

std::shared_ptr<ObjectTypeInfo> ObjectManager::GetType(USHORT index) {
	if (s_types.empty())
		EnumTypes();
	return s_typesMap.at(index);
}

CString ObjectManager::GetObjectName(HANDLE hObject, ULONG pid, USHORT type) {
	HANDLE hDup = DriverHelper::DupHandle(hObject, pid, READ_CONTROL);
	CString name;
	if (hDup) {
		name = GetObjectName(hDup, type);
		::CloseHandle(hDup);
	}

	return name;
}
