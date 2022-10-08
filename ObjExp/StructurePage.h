#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"
#include "ObjectManager.h"
#include "TreeListView.h"
#include "DiaHelper.h"

class CStructPage :
	public CDialogImpl<CStructPage>,
	public CDialogHelper<CStructPage>,
	public CDynamicDialogLayout<CStructPage> {
public:
	enum { IDD = IDD_STRUCT };

	explicit CStructPage(HANDLE hObject);

	void SetSymbol(DiaSymbol sym, PVOID address = nullptr);

	BEGIN_MSG_MAP(CStructPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CStructPage>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

private:
	DiaSymbol m_Object;
	PVOID m_Address;
	HANDLE m_hObject;
	CTreeListView m_Tree;
	CFont m_font;
};


