#include "pch.h"
#include "ObjectHelpers.h"
#include "GenericPage.h"
#include "ObjectPropertiesDlg.h"
#include "NtDll.h"
#include "ProcessHelper.h"
#include <atltime.h>
#include "StringHelper.h"
#include "HandlesPage.h"
#include "ObjectManager.h"
#include "StructurePage.h"
#include "SymbolManager.h"
#include "DriverHelper.h"

UINT ObjectHelpers::ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name, PCWSTR target, DWORD handleCount) {
	CString title = typeName;
	if (name && name[0])
		title += L" (" + CString(name) + L")";
	CObjectPropertiesDlg dlg((PCWSTR)title, typeName);
	CGenericPage page1(hObject, typeName, name, target);
	page1.Create(::GetActiveWindow());
	handleCount = page1.GetHandleCount();
	dlg.AddPage(L"General", page1);
	CHandlesPage page2(hObject, typeName, handleCount);
	if (handleCount) {
		CWaitCursor wait;	// handle count may be large
		page2.Create(::GetActiveWindow());
		dlg.AddPage(L"Handles", page2);
	}
	CStructPage page3(hObject);
	if(auto it = KernelTypes.find(typeName); it != KernelTypes.end()) {
		auto sym = SymbolManager::Get().GetSymbol(it->second);
		if (sym) {
			page3.SetSymbol(std::move(sym), DriverHelper::GetObjectAddress(hObject));
			page3.Create(::GetActiveWindow());
			dlg.AddPage(L"Object", page3);
		}
	}
	dlg.DoModal();

	return 0;
}

std::vector<std::pair<CString, CString>> ObjectHelpers::GetSimpleProps(HANDLE hObject, PCWSTR type, PCWSTR name, PCWSTR target) {
	std::vector<std::pair<CString, CString>> props;
	props.reserve(4);
	CString text;
	if (::_wcsicmp(type, L"Mutant") == 0) {
		NT::MUTANT_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryMutant(hObject, NT::MutantBasicInformation, &info, sizeof(info), nullptr))) {
			props.push_back({ L"Held:", info.CurrentCount <= 0 ? L"Yes" : L"No" });
			props.push_back({ L"Abandoned:", info.AbandonedState ? L"Yes" : L"No" });
		}
		NT::MUTANT_OWNER_INFORMATION owner;
		if (NT_SUCCESS(NT::NtQueryMutant(hObject, NT::MutantOwnerInformation, &owner, sizeof(owner), nullptr))) {
			if (owner.ClientId.UniqueThread) {
				auto pid = HandleToUlong(owner.ClientId.UniqueProcess);
				text.Format(L"%u (%s)", pid, (PCWSTR)ProcessHelper::GetProcessName(pid));
				props.push_back({ L"Owner PID:", text });
				props.push_back({ L"Owner TID:", std::to_wstring(HandleToUlong(owner.ClientId.UniqueThread)).c_str() });
			}
		}
	}
	else if (::_wcsicmp(type, L"Event") == 0) {
		NT::EVENT_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryEvent(hObject, NT::EventBasicInformation, &info, sizeof(info), nullptr))) {
			props.push_back({ L"Type:", info.EventType == NT::NotificationEvent ? L"Notification (Manual Reset)" : L"Synchronization (Auto Reset)" });
			props.push_back({ L"Signaled:", info.EventState ? L"Yes" : L"No" });
		}
	}
	else if (::_wcsicmp(type, L"Semaphore") == 0) {
		NT::SEMAPHORE_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQuerySemaphore(hObject, NT::SemaphoreBasicInformation, &info, sizeof(info), nullptr))) {
			text.Format(L"%u (0x%X)", info.CurrentCount, info.CurrentCount);
			props.push_back({ L"Count:", text });
			text.Format(L"%u (0x%X)", info.MaximumCount, info.MaximumCount);
			props.push_back({ L"Maximum:", text });
		}
	}
	else if (::_wcsicmp(type, L"SymbolicLink") == 0) {
		NT::OBJECT_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryObject(hObject, NT::ObjectBasicInformation, &info, sizeof(info), nullptr))) {
			CString starget(target);
			if (starget.IsEmpty()) {
				auto buffer = std::make_unique<BYTE[]>(1 << 12);
				if (buffer) {
					UNICODE_STRING result;
					result.MaximumLength = 1 << 12;
					result.Buffer = (PWSTR)buffer.get();
					auto status = NT::NtQuerySymbolicLinkObject(hObject, &result, nullptr);
					if (NT_SUCCESS(status))
						starget.SetString(result.Buffer, result.Length / sizeof(WCHAR));
				}
			}
			props.push_back({ L"Target:", starget });
			props.push_back({ L"Creation Time:", CTime(*(FILETIME*)&info.CreationTime).Format(L"%c") });
		}
	}
	else if (::_wcsicmp(type, L"Section") == 0) {
		NT::SECTION_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQuerySection(hObject, NT::SectionBasicInformation, &info, sizeof(info), nullptr))) {
			text.Format(L"0x%llX Bytes", info.MaximumSize.QuadPart);
			props.push_back({ L"Size:", text });
			text.Format(L"0x%08X (%s)", info.AllocationAttributes, (PCWSTR)StringHelper::SectionAttributesToString(info.AllocationAttributes));
			props.push_back({ L"Attributes:", text });
		}
	}
	else if (::_wcsicmp(type, L"Type") == 0) {
		ObjectManager::EnumTypes();
		auto info = ObjectManager::GetType(name);
		ATLASSERT(info);
		if (info) {
			text.Format(L"%u", info->TotalNumberOfObjects);
			props.push_back({ L"Objects: ", text });
			text.Format(L"%u", info->TotalNumberOfHandles);
			props.push_back({ L"Handles: ", text });
		}
	}
	else if (::_wcsicmp(type, L"Process") == 0) {
		auto pid = ::GetProcessId(hObject);
		auto name = ProcessHelper::GetProcessName2(pid);
		props.push_back({ L"Process ID: ", std::to_wstring(pid).c_str() });
		if (!name.IsEmpty())
			props.push_back({ L"Image Name: ", name });
		FILETIME create, exit, kernel, user;
		if (::GetProcessTimes(hObject, &create, &exit, &kernel, &user)) {
			props.push_back({ L"Started: ", CTime(create).Format(L"%c") });
			auto total = (*(ULONGLONG*)&kernel + *(ULONGLONG*)&user) / 10000;	// msec
			auto seconds = CTimeSpan(total / 1000).Format(L"%H:%M:%S");
			props.push_back({ L"CPU Time: ", std::format(L"{}.{}", (PCWSTR)seconds, total % 1000).c_str() });
			if (::WaitForSingleObject(hObject, 0) == WAIT_OBJECT_0) {
				//
				// process dead
				//
				props.push_back({ L"Exited: ", CTime(exit).Format(L"%c") });
			}
		}
	}
	else if (::_wcsicmp(type, L"Thread") == 0) {
		auto name = ProcessHelper::GetProcessName2(::GetProcessIdOfThread(hObject));
		if (!name.IsEmpty())
			props.push_back({ L"Process Image Name: ", name });
		FILETIME create, exit, kernel, user;
		if (::GetThreadTimes(hObject, &create, &exit, &kernel, &user)) {
			props.push_back({ L"Started: ", CTime(create).Format(L"%c") });
			auto total = (*(ULONGLONG*)&kernel + *(ULONGLONG*)&user) / 10000;	// msec
			auto seconds = CTimeSpan(total / 1000).Format(L"%H:%M:%S");
			props.push_back({ L"CPU Time: ", std::format(L"{}.{:03}", (PCWSTR)seconds, total % 1000).c_str() });
			if (::WaitForSingleObject(hObject, 0) == WAIT_OBJECT_0) {
				//
				// thread dead
				//
				props.push_back({ L"Exited: ", CTime(exit).Format(L"%c") });
			}
		}
	}
	else if (::_wcsicmp(type, L"Job") == 0) {
		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
		if (::QueryInformationJobObject(hObject, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr)) {
			props.push_back({ L"Active Processes: ", std::to_wstring(info.ActiveProcesses).c_str() });
			props.push_back({ L"Total Processes: ", std::to_wstring(info.TotalProcesses).c_str() });
			auto total = (info.TotalUserTime.QuadPart + info.TotalKernelTime.QuadPart) / 10000;
			auto seconds = CTimeSpan(total / 1000).Format(L"%H:%M:%S");
			props.push_back({ L"CPU Time: ", std::format(L"{}.{:03}", (PCWSTR)seconds, total % 1000).c_str() });
			props.push_back({ L"Page Faults: ", std::to_wstring(info.TotalPageFaultCount).c_str() });
		}
	}

	return props;
}

bool ObjectHelpers::IsNamedObjectType(USHORT index) {
	auto type = ObjectManager::GetType(index)->TypeName;
	static const CString nonNamed[] = {
		L"Process", L"Thread", L"Token", L"EtwRegistration", L"IoCompletion",
		L"WaitCompletionPacket", L"TpWorkerFactory",
	};

	return std::find(std::begin(nonNamed), std::end(nonNamed), type) == std::end(nonNamed);
}

HANDLE ObjectHelpers::OpenObject(PCWSTR name, PCWSTR typeName, DWORD access) {
	HANDLE hObject = nullptr;
	CString type(typeName);
	OBJECT_ATTRIBUTES attr;
	UNICODE_STRING uname;
	RtlInitUnicodeString(&uname, name);
	InitializeObjectAttributes(&attr, &uname, 0, nullptr, nullptr);
	auto status = STATUS_UNSUCCESSFUL;

	if (type == L"Event")
		status = NT::NtOpenEvent(&hObject, access, &attr);
	else if (type == L"Mutant")
		status = NT::NtOpenMutant(&hObject, access, &attr);
	else if (type == L"Section")
		status = NT::NtOpenSection(&hObject, access, &attr);
	else if (type == L"Session")
		status = NT::NtOpenSession(&hObject, access, &attr);
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
	else if (type == L"File" || type == L"Device") {
		IO_STATUS_BLOCK ioStatus;
		status = NT::NtOpenFile(&hObject, access, &attr, &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
	}

	return hObject;
}
