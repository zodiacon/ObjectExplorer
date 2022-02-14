#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"

class CGenericPropertiesPage : 
	public CDialogImpl<CGenericPropertiesPage>,
	public CDialogHelper<CGenericPropertiesPage>,
	public CDynamicDialogLayout<CGenericPropertiesPage> {
public:
	enum { IDD = IDD_PROPERTIES };

	CGenericPropertiesPage(HANDLE hObject, PCWSTR typeName, PCWSTR name) : m_hObject(hObject), m_TypeName(typeName), m_Name(name) {}

	BEGIN_MSG_MAP(CGenericPropertiesPage)
		//MESSAGE_HANDLER(WM_CTLCOLORDLG, OnDialogColor)
		COMMAND_ID_HANDLER(IDC_SECURITY, OnEditSecurity)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CGenericPropertiesPage>)
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
};


