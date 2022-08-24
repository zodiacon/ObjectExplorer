#pragma once

#include "resource.h"
#include "DialogHelper.h"
#include "ResourceManager.h"
#include <VirtualListView.h>
#include "ObjectManager.h"

class CHandlesPage :
	public CDialogImpl<CHandlesPage>,
	public CDialogHelper<CHandlesPage>,
	public CVirtualListView<CHandlesPage>,
	public CDynamicDialogLayout<CHandlesPage> {
public:
	enum { IDD = IDD_HANDLES };

	CHandlesPage(HANDLE hObject, PCWSTR typeName) : m_hObject(hObject), m_TypeName(typeName) {}

	CString GetColumnText(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	int GetRowImage(HWND, int, int) const;

	BEGIN_MSG_MAP(CHandlesPage)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CVirtualListView<CHandlesPage>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CHandlesPage>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDialogColor(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	enum class ColumnType {
		PID, Handle, ProcessName, Attributes, Access, DecodedAccess,
	};

private:
	CListViewCtrl m_List;
	std::vector<HandleInfo> m_Handles;
	HANDLE m_hObject;
	CString m_TypeName;
};


