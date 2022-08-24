#include "pch.h"
#include "GenericPage.h"
#include "SecurityInfo.h"
#include "DriverHelper.h"
#include "ObjectHelpers.h"
#include "ObjectManager.h"
#include "NtDll.h"
#include <ThemeHelper.h>

LRESULT CGenericPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	ATLASSERT(!m_TypeName.IsEmpty());
	InitDynamicLayout(false, false);
	AddIconToButton(IDC_SECURITY, IDI_SHIELD2);

	NT::OBJECT_BASIC_INFORMATION info;
	PVOID address{ nullptr };
	if (m_hObject == nullptr && m_TypeName == L"Type") {
		ObjectManager mgr;
		auto type = mgr.GetType(m_Name);
		SetDlgItemInt(IDC_PAGED, type->DefaultPagedPoolCharge);
		SetDlgItemInt(IDC_NPAGED, type->DefaultNonPagedPoolCharge);
		SetDlgItemText(IDC_HANDLES, L"");
		SetDlgItemText(IDC_REFS, L"");
		GetDlgItem(IDC_SECURITY).EnableWindow(FALSE);
	}
	else if (m_hObject && STATUS_SUCCESS == NT::NtQueryObject(m_hObject, NT::ObjectBasicInformation, &info, sizeof(info), nullptr)) {
		SetDlgItemInt(IDC_HANDLES, info.HandleCount - 1);	// subtract our own handle?
		SetDlgItemInt(IDC_REFS, info.PointerCount);
		SetDlgItemInt(IDC_PAGED, info.PagedPoolCharge);
		SetDlgItemInt(IDC_NPAGED, info.NonPagedPoolCharge);
		address = DriverHelper::GetObjectAddress(m_hObject);
		if (address) {
			CString text;
			text.Format(L"0x%p", address);
			SetDlgItemText(IDC_ADDRESS, text);
		}
		if (info.Attributes & OBJ_PERMANENT)
			CheckDlgButton(IDC_PERMANENT, BST_CHECKED);
		if (info.Attributes & OBJ_EXCLUSIVE)
			CheckDlgButton(IDC_EXCLUSIVE, BST_CHECKED);
	}
	else {
		SetDlgItemText(IDC_HANDLES, L"");
		SetDlgItemText(IDC_REFS, L"");
		SetDlgItemText(IDC_PAGED, L"");
		SetDlgItemText(IDC_NPAGED, L"");
		GetDlgItem(IDC_SECURITY).EnableWindow(FALSE);
	}
	if (!address) {
		GetDlgItem(IDC_ADDRESS).ShowWindow(SW_HIDE);
		GetDlgItem(IDC_ADDRESSLABEL).ShowWindow(SW_HIDE);
	}

	SetDlgItemText(IDC_TYPE, m_TypeName);
	SetDlgItemText(IDC_NAME, m_Name);
	SetDialogIcon(ResourceManager::Get().GetTypeIcon(m_TypeName));

	auto props = ObjectHelpers::GetSimpleProps(m_hObject, m_TypeName, m_Name, m_Target);
	if (!props.empty()) {
		SetDlgItemText(IDC_FRAME, m_TypeName + L" Info");
		int i = 0;
		for (auto& [label, text] : props) {
			SetDlgItemText(IDC_LABEL + i, label);
			SetDlgItemText(IDC_VALUE + i, text);
			i++;
		}
	}
	else {
		GetDlgItem(IDC_FRAME).ShowWindow(SW_HIDE);
	}
	return 0;
}

LRESULT CGenericPage::OnDialogColor(UINT, WPARAM, LPARAM, BOOL&) {
	return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
}

LRESULT CGenericPage::OnEditSecurity(WORD, WORD, HWND, BOOL&) {
	SecurityInfo si(m_hObject, m_Name);
	ThemeHelper::Suspend();
	::EditSecurity(m_hWnd, &si);
	ThemeHelper::Resume();

	return 0;
}
