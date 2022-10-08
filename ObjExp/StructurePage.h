#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"
#include "ObjectManager.h"
#include "TreeListView.h"

class DiaSymbol;

class CStructPage :
	public CDialogImpl<CStructPage>,
	public CDialogHelper<CStructPage>,
	public CDynamicDialogLayout<CStructPage> {
public:
	enum { IDD = IDD_STRUCT };

	CStructPage(HANDLE hObject, DiaSymbol const& sym);

	//CString GetColumnText(HWND, int row, int col) const;
	//void DoSort(SortInfo const* si);
	//int GetRowImage(HWND, int, int) const;

	BEGIN_MSG_MAP(CStructPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CStructPage>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	enum class ColumnType {
		PID, Handle, ProcessName, Attributes, Access, DecodedAccess,
	};

private:
	DiaSymbol const& m_Object;
	HANDLE m_hObject;
	CString m_TypeName;
	CTreeListView m_Tree;
};


