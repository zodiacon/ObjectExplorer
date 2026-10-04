#pragma once

#include "resource.h"
#include <DialogHelper.h>

class CFindDlg :
	public CDialogImpl<CFindDlg>,
	public CDialogHelper<CFindDlg> {
public:
	enum { IDD = IDD_FIND };

	CString const& GetText() const;
	bool IsMatchCase() const;

	BEGIN_MSG_MAP(CFindDlg)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_HANDLER(IDC_TEXT, EN_CHANGE, OnTextChanged)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
	END_MSG_MAP()

private:
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnTextChanged(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	// the last search is offered again
	inline static CString s_Text;
	inline static bool s_MatchCase{ false };
};
