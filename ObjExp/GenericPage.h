#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"

class CGenericPage : 
	public CDialogImpl<CGenericPage>,
	public CDialogHelper<CGenericPage>,
	public CDynamicDialogLayout<CGenericPage> {
public:
	enum { IDD = IDD_PROPERTIES };

	CGenericPage(HANDLE hObject, PCWSTR typeName, PCWSTR name, PCWSTR target) : 
		m_hObject(hObject), m_TypeName(typeName), m_Name(name), m_Target(target) {}

	BEGIN_MSG_MAP(CGenericPage)
		COMMAND_ID_HANDLER(IDC_SECURITY, OnEditSecurity)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CGenericPage>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDialogColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditSecurity(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

protected:
	HANDLE m_hObject;
	CString m_TypeName;
	CString m_Name;
	CString m_Target;
};


