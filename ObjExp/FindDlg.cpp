#include "pch.h"
#include "FindDlg.h"

CString const& CFindDlg::GetText() const {
	return s_Text;
}

bool CFindDlg::IsMatchCase() const {
	return s_MatchCase;
}

LRESULT CFindDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
	SetDialogIcon(IDI_FIND);
	SetDlgItemText(IDC_TEXT, s_Text);
	CheckDlgButton(IDC_MATCHCASE, s_MatchCase ? BST_CHECKED : BST_UNCHECKED);
	CEdit edit(GetDlgItem(IDC_TEXT));
	edit.SetSelAll();
	edit.SetFocus();
	GetDlgItem(IDOK).EnableWindow(!s_Text.IsEmpty());
	CenterWindow(GetParent());
	return FALSE;
}

LRESULT CFindDlg::OnTextChanged(WORD, WORD, HWND, BOOL&) {
	GetDlgItem(IDOK).EnableWindow(GetDlgItem(IDC_TEXT).GetWindowTextLength() > 0);
	return 0;
}

LRESULT CFindDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
	if (wID == IDOK) {
		GetDlgItemText(IDC_TEXT, s_Text);
		s_Text.Trim();
		if (s_Text.IsEmpty())
			return 0;
		s_MatchCase = IsDlgButtonChecked(IDC_MATCHCASE) == BST_CHECKED;
	}
	EndDialog(wID);
	return 0;
}
