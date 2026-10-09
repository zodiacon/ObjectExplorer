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
#include "ObjectTypePage.h"
#include "TypeProperties.h"
#include "SymbolManager.h"
#include "DriverHelper.h"
#include "SecurityInfo.h"
#include <WTLHelper.h>

UINT ObjectHelpers::ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name, PCWSTR target, DWORD handleCount, DWORD pid) {
	CString title = typeName;
	if (name && name[0])
		title += L" (" + CString(name) + L")";
	CObjectPropertiesDlg dlg((PCWSTR)title, typeName);
	CGenericPage page1(hObject, typeName, name, target);
	page1.Create(::GetActiveWindow());
	handleCount = page1.GetHandleCount();
	dlg.AddPage(L"General", page1, CObjectPropertiesDlg::GeneralImage);
	CObjectTypePage typePage(hObject, typeName, pid);
	if (hObject && TypeProperties::HasProperties(typeName)) {
		typePage.Create(::GetActiveWindow());
		dlg.AddPage(TypeProperties::GetPageTitle(typeName), typePage, CObjectPropertiesDlg::TypeImage);
	}
	CHandlesPage page2(hObject, typeName, handleCount);
	if (handleCount) {
		CWaitCursor wait;	// handle count may be large
		page2.Create(::GetActiveWindow());
		dlg.AddPage(L"Handles", page2, CObjectPropertiesDlg::HandlesImage);
	}
	CStructPage page3(hObject);
	if(auto it = KernelTypes.find(typeName); it != KernelTypes.end()) {
		auto sym = SymbolManager::Get().GetSymbol(it->second);
		if (sym) {
			page3.SetSymbol(std::move(sym), DriverHelper::GetObjectAddress(hObject));
			page3.Create(::GetActiveWindow());
			dlg.AddPage(L"Object", page3, CObjectPropertiesDlg::ObjectImage);
		}
	}
	dlg.DoModal();

	return 0;
}

std::vector<std::pair<CString, CString>> ObjectHelpers::GetSimpleProps(HANDLE hObject, PCWSTR type, PCWSTR name, PCWSTR target) {
	std::vector<std::pair<CString, CString>> props;
	props.reserve(4);
	CString text;
	// Event, Mutant, Semaphore, Timer, Section, Process, Thread and Job have pages of their own (see TypeProperties)
	if (::_wcsicmp(type, L"SymbolicLink") == 0) {
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

bool ObjectHelpers::ShowNamespaceObjectProperties(HWND hParent, PCWSTR fullName, PCWSTR type, PCWSTR target) {
	if (::_wcsicmp(type, L"Type") == 0) {
		//
		// type objects can't be opened; their properties come from the type information
		//
		auto name = wcsrchr(fullName, L'\\');
		ShowObjectProperties(nullptr, type, name ? name + 1 : fullName);
		return true;
	}
	HANDLE hObject{ nullptr };
	ObjectManager::OpenObject(fullName, type, hObject);
	if (hObject) {
		ShowObjectProperties(hObject, type, fullName, target);
		::CloseHandle(hObject);
		return true;
	}
	AtlMessageBox(hParent, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
	return false;
}

bool ObjectHelpers::EditNamespaceObjectSecurity(HWND hParent, PCWSTR fullName, PCWSTR type) {
	HANDLE hObject{ nullptr };
	ObjectManager::OpenObject(fullName, type, hObject, READ_CONTROL | WRITE_DAC | WRITE_OWNER);
	bool readOnly = hObject == nullptr;
	if (readOnly) {
		// can still view it
		ObjectManager::OpenObject(fullName, type, hObject, READ_CONTROL);
	}
	if (!hObject) {
		AtlMessageBox(hParent, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
		return false;
	}
	SecurityInfo si(hObject, fullName, readOnly);
	WTLHelper::SuspendHook();
	::EditSecurity(hParent, &si);
	WTLHelper::ResumeHook();
	::CloseHandle(hObject);
	return true;
}
