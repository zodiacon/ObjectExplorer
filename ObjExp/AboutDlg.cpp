// aboutdlg.cpp : implementation of the CAboutDlg class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include <VersionResourceHelper.h>

LRESULT CAboutDlg::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	SetDialogIcon(IDR_MAINFRAME);

	CenterWindow(GetParent());
	VersionResourceHelper vh;
	auto version = vh.GetValue(L"ProductVersion");
	SetDlgItemText(IDC_VERSION, L"Object Explorer v" + version);
	SetDlgItemText(IDC_COPYRIGHT, vh.GetValue(L"LegalCopyright"));

	return TRUE;
}

LRESULT CAboutDlg::OnCloseCmd(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	EndDialog(wID);
	return 0;
}

LRESULT CAboutDlg::OnClickSyslink(int, LPNMHDR hdr, BOOL&) const {
	CString text;
	GetDlgItem((UINT)hdr->idFrom).GetWindowText(text);
	text.Replace(L"<a>", L"");
	text.Replace(L"</a>", L"");
	::ShellExecute(nullptr, L"open", text, nullptr, nullptr, SW_SHOWDEFAULT);
	return 0;
}
