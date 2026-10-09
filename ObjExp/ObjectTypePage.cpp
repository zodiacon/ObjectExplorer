#include "pch.h"
#include "ObjectTypePage.h"
#include "ResourceManager.h"

LRESULT CObjectTypePage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	InitDynamicLayout(false);
	AddIconToButton(IDC_REFRESH, IDI_REFRESH);

	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
	m_List.InsertColumn(0, L"Property", LVCFMT_LEFT, 150);
	m_List.InsertColumn(1, L"Value", LVCFMT_LEFT, 350);

	Refresh();
	return 0;
}

LRESULT CObjectTypePage::OnRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

void CObjectTypePage::Refresh() {
	m_List.SetRedraw(FALSE);
	m_List.DeleteAllItems();
	auto props = TypeProperties::GetProperties(m_hObject, m_TypeName, m_Pid);
	if (props.empty())
		props.push_back({ L"Error", L"Insufficient access to query the object" });
	int i = 0;
	for (auto& [name, value] : props) {
		m_List.InsertItem(i, name);
		m_List.SetItemText(i, 1, value);
		i++;
	}
	m_List.SetRedraw(TRUE);
}
