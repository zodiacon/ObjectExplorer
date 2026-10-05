#pragma once

#include "resource.h"
#include <DialogHelper.h>

//
// where the kernel's symbols (for the Object tab) are looked for: a symbol server with a local cache,
// additional paths and _NT_SYMBOL_PATH
//
class CSymbolSettingsDlg :
	public CDialogImpl<CSymbolSettingsDlg>,
	public CDialogHelper<CSymbolSettingsDlg> {
public:
	enum { IDD = IDD_SYMBOLS };

	BEGIN_MSG_MAP(CSymbolSettingsDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOK)
		COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
		COMMAND_ID_HANDLER(IDC_SYM_BROWSE, OnBrowse)
		COMMAND_ID_HANDLER(IDC_SYM_DEFAULTS, OnDefaults)
		COMMAND_ID_HANDLER(IDC_SYM_USE_SERVER, OnChanged)
		COMMAND_ID_HANDLER(IDC_SYM_USE_ENV, OnChanged)
		COMMAND_HANDLER(IDC_SYM_SERVER_URL, EN_CHANGE, OnChanged)
		COMMAND_HANDLER(IDC_SYM_CACHE, EN_CHANGE, OnChanged)
		COMMAND_HANDLER(IDC_SYM_EXTRA, EN_CHANGE, OnChanged)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOK(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnBrowse(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDefaults(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	void Load(bool useServer, std::wstring const& url, std::wstring const& cache, std::wstring const& extra, bool useEnv);
	void UpdateUI();
	CString GetText(UINT id) const;
};
