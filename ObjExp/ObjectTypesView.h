#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"
#include "ObjectManager.h"
#include "TimerManager.h"

class CObjectTypesView : 
	public CViewBase<CObjectTypesView>,
	public CCustomDraw<CObjectTypesView>,
	public CTimerManager<CObjectTypesView>,
	public CVirtualListView<CObjectTypesView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);

	void OnFinalMessage(HWND /*hWnd*/) override;
	CString GetTitle() const override;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnSubItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);

	void UpdateUI(CUpdateUIBase& ui);
	void DoTimerUpdate();
	void OnPageActivated(bool activate);

protected:
	BEGIN_MSG_MAP(CObjectTypesView)
		NOTIFY_CODE_HANDLER(LVN_ITEMCHANGED, OnStateChanged)
		NOTIFY_CODE_HANDLER(LVN_ODSTATECHANGED, OnStateChanged)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CTimerManager<CObjectTypesView>)
		CHAIN_MSG_MAP(CCustomDraw<CObjectTypesView>)
		CHAIN_MSG_MAP(CViewBase<CObjectTypesView>)
		CHAIN_MSG_MAP(CVirtualListView<CObjectTypesView>)
	ALT_MSG_MAP(1)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		CHAIN_MSG_MAP_ALT(CTimerManager<CObjectTypesView>, 1)
	END_MSG_MAP()

private:
	enum class ColumnType {
		None,
		Name, Index, Handles, Objects, PeakHandles, PeakObjects, Pool,
		DefaultPaged, DefaultNonPaged, ValidAccess, InvalidAttributes,
		GenericRead, GenericWrite, GenericExecute, GenericAll,	
	};

	ColumnType MapChangeToColumn(ObjectManager::ChangeType type) const;

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnStateChanged(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	ObjectManager m_mgr;
	std::vector<std::shared_ptr<ObjectTypeInfo>> m_Items;
};
