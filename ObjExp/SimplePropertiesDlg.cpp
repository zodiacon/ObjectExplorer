#include "pch.h"
#include "SimplePropertiesDlg.h"

CSimplePropertiesPage::CSimplePropertiesPage(std::vector<std::pair<CString, CString>> const& properties) : m_Properties(properties) {
}

LRESULT CSimplePropertiesPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	int i = 0;
	for (auto& [label, text] : m_Properties) {
		SetDlgItemText(IDC_LABEL + i, label);
		SetDlgItemText(IDC_VALUE + i, text);
		i++;
	}
	return 0;
}

LRESULT CSimplePropertiesPage::OnDialogColor(UINT, WPARAM, LPARAM, BOOL&) {
	return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
}
