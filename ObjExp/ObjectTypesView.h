#pragma once

#include "ViewBase.h"
#include "VirtualListView.h"
#include "ObjectManager.h"

class CObjectTypesView : 
	public CViewBase<CObjectTypesView>,
	public CCustomDraw<CObjectTypesView>,
	public CVirtualListView<CObjectTypesView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);

	void OnFinalMessage(HWND /*hWnd*/) override;
	CString GetTitle() const override;
	int GetIconIndex() const override;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);

protected:
	BEGIN_MSG_MAP(CObjectTypesView)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CCustomDraw<CObjectTypesView>)
		CHAIN_MSG_MAP(CViewBase<CObjectTypesView>)
		CHAIN_MSG_MAP(CVirtualListView<CObjectTypesView>)

	END_MSG_MAP()

private:
	enum class ColumnType {
		Name, Index, Handles, Objects, PeakHandles, PeakObjects, Pool,
		DefaultPaged, DefaultNonPaged, ValidAccess, InvalidAttributes,
		GenericRead, GenericWrite, GenericExecute, GenericAll,	
	};

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	ObjectManager m_mgr;
	std::vector<std::shared_ptr<ObjectTypeInfo>> m_Items;
};
