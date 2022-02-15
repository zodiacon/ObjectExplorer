#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"

class CSimplePropertiesPage :
	public CDialogImpl<CSimplePropertiesPage>,
	public CDialogHelper<CSimplePropertiesPage>,
	public CDynamicDialogLayout<CSimplePropertiesPage> {
public:
	enum { IDD = IDD_SIMPLEPROP };

	CSimplePropertiesPage(std::vector<std::pair<CString, CString>> const& properties);

	BEGIN_MSG_MAP(CSimplePropertiesPage)
		MESSAGE_HANDLER(WM_CTLCOLORDLG, OnDialogColor)
		MESSAGE_HANDLER(WM_CTLCOLOREDIT, OnDialogColor)
		MESSAGE_HANDLER(WM_CTLCOLORSTATIC, OnDialogColor)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CSimplePropertiesPage>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDialogColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

protected:
	std::vector<std::pair<CString, CString>> m_Properties;
};


