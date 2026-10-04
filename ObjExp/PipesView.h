#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"

//
// named pipes or mailslots: these live in their file systems (\Device\NamedPipe, \Device\Mailslot),
// not in the object manager namespace
//
class CPipesView :
	public CViewBase<CPipesView>,
	public CVirtualListView<CPipesView> {
public:
	CPipesView(IMainFrame* frame, bool mailslots);

	CString GetTitle() const override;
	void Refresh() override;
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnPageActivated(bool active);

	BEGIN_MSG_MAP(CPipesView)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_PIPELIST_FINDHANDLES, OnFindHandles)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CViewBase<CPipesView>)
		CHAIN_MSG_MAP(CVirtualListView<CPipesView>)
	END_MSG_MAP()

private:
	struct Item {
		CString Name;
		LONGLONG Instances{ 0 }, MaxInstances{ 0 };
	};

	enum class ColumnType {
		None,
		Name, Instances, MaxInstances, FullName,
	};

	CString GetFullName(Item const& item) const;
	void FindHandles(int row) const;
	void ShowProperties(int row) const;
	void UpdateStatusText() const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnFindHandles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<Item> m_Items;
	bool m_Mailslots;
};
