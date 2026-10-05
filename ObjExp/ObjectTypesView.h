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

	CString GetTitle() const override;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnSubItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);

	void UpdateUI(bool force = false);
	void DoTimerUpdate();
	void OnPageActivated(bool activate);

	BEGIN_MSG_MAP(CObjectTypesView)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_TYPESLIST_ALLHANDLES, OnShowAllHandles)
		COMMAND_ID_HANDLER(ID_TYPESLIST_ALLOBJECTS, OnShowAllObjects)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(::RegisterWindowMessage(L"WTLHelperUpdateTheme"), OnUpdateTheme)
		MESSAGE_HANDLER(WM_UPDATE_DARKMODE, OnUpdateTheme)
		CHAIN_MSG_MAP(CTimerManager<CObjectTypesView>)
		CHAIN_MSG_MAP(CCustomDraw<CObjectTypesView>)
		CHAIN_MSG_MAP(CViewBase<CObjectTypesView>)
		CHAIN_MSG_MAP(CVirtualListView<CObjectTypesView>)
		CHAIN_MSG_MAP_ALT(CTimerManager<CObjectTypesView>, 1)
	END_MSG_MAP()

private:
	void UpdateStatusText();

	enum class ColumnType {
		None,
		Name, Index, Handles, Objects, PeakHandles, PeakObjects, Pool,
		DefaultPaged, DefaultNonPaged, ValidAccess, InvalidAttributes,
		GenericRead, GenericWrite, GenericExecute, GenericAll,	
	};

	ColumnType MapChangeToColumn(ObjectManager::ChangeType type) const;
	void UpdateChanges();

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnPauseResume(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowAllHandles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnShowAllObjects(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnUpdateTheme(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	struct TypeCounts {
		uint32_t Handles, Objects, PeakHandles, PeakObjects;
	};
	//
	// counts as of this view's previous update: ObjectManager's own change list is reset by every
	// EnumTypes caller (e.g. the main window's status bar timer), so the view tracks its own changes
	//
	std::unordered_map<ObjectTypeInfo const*, TypeCounts> m_LastCounts;
	std::vector<ObjectManager::Change> m_Changes;
	std::vector<std::shared_ptr<ObjectTypeInfo>> m_Items;
	COLORREF m_Green, m_Red;
};
