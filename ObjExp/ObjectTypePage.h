#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "TypeProperties.h"

//
// properties specific to the object's type (see TypeProperties)
//
class CObjectTypePage :
	public CDialogImpl<CObjectTypePage>,
	public CDialogHelper<CObjectTypePage>,
	public CDynamicDialogLayout<CObjectTypePage> {
public:
	enum { IDD = IDD_TYPEINFO };

	// pid: the process the handle was duplicated from, if any
	CObjectTypePage(HANDLE hObject, PCWSTR typeName, DWORD pid = 0) : m_hObject(hObject), m_TypeName(typeName), m_Pid(pid) {}

	BEGIN_MSG_MAP(CObjectTypePage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		COMMAND_ID_HANDLER(IDC_REFRESH, OnRefresh)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CObjectTypePage>)
	END_MSG_MAP()

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void Refresh();

	CListViewCtrl m_List;
	HANDLE m_hObject;
	CString m_TypeName;
	DWORD m_Pid;
};
