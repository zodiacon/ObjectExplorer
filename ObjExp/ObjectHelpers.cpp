#include "pch.h"
#include "ObjectHelpers.h"
#include "GenericObjectProperties.h"
#include "ObjectPropertiesDlg.h"
#include "NtDll.h"
#include "SimplePropertiesDlg.h"
#include "ProcessHelper.h"
#include <atltime.h>

UINT ObjectHelpers::ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name, PCWSTR target) {
	CString title = CString(typeName) + CString(name ? (L" (" + CString(name) + L")") : CString());
	CObjectPropertiesDlg dlg((PCWSTR)title, typeName);
	CGenericPropertiesPage page1(hObject, typeName, name);
	page1.Create(::GetActiveWindow());
	auto props = GetSimpleProps(hObject, typeName, name, target);
	CSimplePropertiesPage page2(props);
	dlg.AddPage(L"General", page1);
	if (!props.empty()) {
		page2.Create(::GetActiveWindow());
		dlg.AddPage(typeName, page2);
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
				text.Format(L"%u (%s)", pid, ProcessHelper::GetProcessName(pid));
				props.push_back({ L"Owner PID:", text });
				props.push_back({ L"Owner TID:", std::to_wstring(HandleToUlong(owner.ClientId.UniqueThread)).c_str() });
			}
		}
	}
	else if(::_wcsicmp(type, L"Event") == 0) {
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
			props.push_back({ L"Target:", target });
			props.push_back({ L"Creation Time:", CTime(*(FILETIME*)&info.CreationTime).Format(L"%c") });
		}
	}
	else if (::_wcsicmp(type, L"Section") == 0) {
		NT::SECTION_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQuerySection(hObject, NT::SectionBasicInformation, &info, sizeof(info), nullptr))) {
			text.Format(L"0x%llX Bytes", info.MaximumSize.QuadPart);
			props.push_back({ L"Size:", text });
			text.Format(L"0x%08X (%s)", info.AllocationAttributes, SectionAttributesToString(info.AllocationAttributes));
			props.push_back({ L"Attributes:", text });
		}
	}
	return props;
}

CString ObjectHelpers::SectionAttributesToString(DWORD value) {
	CString text;
	struct {
		DWORD attribute;
		PCWSTR text;
	} attributes[] = {
		{ SEC_COMMIT, L"Commit" },
		{ SEC_RESERVE, L"Reserve" },
		{ SEC_IMAGE, L"Image" },
		{ SEC_NOCACHE, L"No Cache" },
		{ SEC_FILE, L"File" },
		{ SEC_WRITECOMBINE, L"Write Combine" },
		{ SEC_PROTECTED_IMAGE, L"Protected Image" },
		{ SEC_LARGE_PAGES, L"Large Pages" },
		{ SEC_IMAGE_NO_EXECUTE, L"No Execute" },
	};

	for (auto& item : attributes)
		if (value & item.attribute)
			(text += item.text) += L", ";
	if (text.GetLength() == 0)
		text = L"None";
	else
		text = text.Left(text.GetLength() - 2);
	return text;
}
