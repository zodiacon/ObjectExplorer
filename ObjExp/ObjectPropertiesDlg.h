#pragma once

#include "DialogHelper.h"
#include "resource.h"

class CObjectPropertiesDlg :
	public CDialogImpl<CObjectPropertiesDlg>,
	public CDialogHelper<CObjectPropertiesDlg>,
	public CDynamicDialogLayout<CObjectPropertiesDlg> {
public:
	CObjectPropertiesDlg(PCWSTR title, PCWSTR typeName) : m_Title(title), m_Type(typeName) {}

	enum { IDD = IDD_OBJECTPROPS };

	bool AddPage(PCWSTR title, HWND hPage);
	bool AddPage(PCWSTR title, HPROPSHEETPAGE hPage);

protected:
	BEGIN_MSG_MAP(CObjectPropertiesDlg)
		NOTIFY_CODE_HANDLER(TCN_SELCHANGE, OnTabChanged)
		MESSAGE_HANDLER(WM_SIZE, OnSize)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDOK, OnOKCancel)
		COMMAND_ID_HANDLER(IDCANCEL, OnOKCancel)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CObjectPropertiesDlg>)
	END_MSG_MAP()

private:
	void UpdateSize();

	LRESULT OnTabChanged(int, LPNMHDR, BOOL&);
	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnOKCancel(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnDialogColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	struct TabItem {
		CString Title;
		CWindow win;
		CPropertyPageWindow page;
	};
	std::vector<TabItem> m_Pages;
	CString m_Title, m_Type;
	CTabCtrl m_Tabs;
	int m_SelectedPage{ -1 }, m_LastActivePage{ -1 };
};
