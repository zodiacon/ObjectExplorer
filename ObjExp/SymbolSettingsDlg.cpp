#include "pch.h"
#include "SymbolSettingsDlg.h"
#include "AppSettings.h"

CString CSymbolSettingsDlg::GetText(UINT id) const {
	CString text;
	GetDlgItemText(id, text);
	return text.Trim();
}

void CSymbolSettingsDlg::Load(bool useServer, std::wstring const& url, std::wstring const& cache, std::wstring const& extra, bool useEnv) {
	CheckDlgButton(IDC_SYM_USE_SERVER, useServer ? BST_CHECKED : BST_UNCHECKED);
	SetDlgItemText(IDC_SYM_SERVER_URL, url.c_str());
	SetDlgItemText(IDC_SYM_CACHE, cache.c_str());
	SetDlgItemText(IDC_SYM_EXTRA, extra.c_str());
	CheckDlgButton(IDC_SYM_USE_ENV, useEnv ? BST_CHECKED : BST_UNCHECKED);
	UpdateUI();
}

void CSymbolSettingsDlg::UpdateUI() {
	bool server = IsDlgButtonChecked(IDC_SYM_USE_SERVER) == BST_CHECKED;
	for (auto id : { IDC_SYM_SERVER_URL, IDC_SYM_CACHE, IDC_SYM_BROWSE })
		GetDlgItem(id).EnableWindow(server);

	auto path = AppSettings::BuildSymbolSearchPath(server, (PCWSTR)GetText(IDC_SYM_SERVER_URL), (PCWSTR)GetText(IDC_SYM_CACHE),
		(PCWSTR)GetText(IDC_SYM_EXTRA), IsDlgButtonChecked(IDC_SYM_USE_ENV) == BST_CHECKED);
	SetDlgItemText(IDC_SYM_PREVIEW, path.empty() ? L"(default: DIA's own search only)" : path.c_str());
}

LRESULT CSymbolSettingsDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetDialogIcon(IDR_MAINFRAME);
	CenterWindow(GetParent());

	auto& s = AppSettings::Get();
	Load(s.SymbolServerEnabled() != 0, s.SymbolServerUrl(), s.SymbolCache(), s.SymbolExtraPaths(), s.SymbolUseEnvPath() != 0);
	return TRUE;
}

LRESULT CSymbolSettingsDlg::OnOK(WORD, WORD wID, HWND, BOOL&) {
	bool server = IsDlgButtonChecked(IDC_SYM_USE_SERVER) == BST_CHECKED;
	auto url = GetText(IDC_SYM_SERVER_URL);
	if (server) {
		bool valid = (url.GetLength() > 7 && url.Left(7).CompareNoCase(L"http://") == 0) ||
			(url.GetLength() > 8 && url.Left(8).CompareNoCase(L"https://") == 0);
		if (!valid) {
			AtlMessageBox(m_hWnd, L"The symbol server URL must start with http:// or https://", IDS_TITLE, MB_ICONWARNING);
			GetDlgItem(IDC_SYM_SERVER_URL).SetFocus();
			return 0;
		}
	}

	auto& s = AppSettings::Get();
	s.SymbolServerEnabled(server ? 1 : 0);
	s.SymbolServerUrl((PCWSTR)GetText(IDC_SYM_SERVER_URL));
	s.SymbolCache((PCWSTR)GetText(IDC_SYM_CACHE));
	s.SymbolExtraPaths((PCWSTR)GetText(IDC_SYM_EXTRA));
	s.SymbolUseEnvPath(IsDlgButtonChecked(IDC_SYM_USE_ENV) == BST_CHECKED ? 1 : 0);
	EndDialog(wID);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnCancel(WORD, WORD wID, HWND, BOOL&) {
	EndDialog(wID);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnBrowse(WORD, WORD, HWND, BOOL&) {
	CShellFileOpenDialog dlg(nullptr, FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
	if (dlg.DoModal(m_hWnd) == IDOK) {
		CString path;
		if (SUCCEEDED(dlg.GetFilePath(path)))
			SetDlgItemText(IDC_SYM_CACHE, path);
	}
	return 0;
}

LRESULT CSymbolSettingsDlg::OnDefaults(WORD, WORD, HWND, BOOL&) {
	Load(true, AppSettings::DefaultSymbolServerUrl, AppSettings::DefaultSymbolCache, L"", true);
	return 0;
}

LRESULT CSymbolSettingsDlg::OnChanged(WORD, WORD, HWND, BOOL&) {
	UpdateUI();
	return 0;
}
