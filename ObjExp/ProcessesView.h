#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"

//
// processes (or threads) of the system, from a Toolhelp snapshot; F5 takes a new one
//
class CProcessesView :
	public CViewBase<CProcessesView>,
	public CVirtualListView<CProcessesView> {
public:
	CProcessesView(IMainFrame* frame, bool threads);

	CString GetTitle() const override;
	void Refresh() override;
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnPageActivated(bool active);

	BEGIN_MSG_MAP(CProcessesView)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CViewBase<CProcessesView>)
		CHAIN_MSG_MAP(CVirtualListView<CProcessesView>)
	END_MSG_MAP()

private:
	struct Item {
		DWORD Id;			// process or thread ID
		DWORD ProcessId;	// owning process (threads)
		DWORD ParentId;		// processes
		DWORD Threads;		// processes
		LONG Priority;
		CString Name;		// process name
		// filled when first needed (opening the process/thread)
		mutable bool DetailsChecked{ false };
		mutable DWORD Session{ (DWORD)-1 };
		mutable DWORD Handles{ 0 };
		mutable CString User, ImagePath;
		mutable FILETIME CreateTime{}, KernelTime{}, UserTime{};
	};

	enum class ColumnType {
		None,
		Name, Id, ProcessId, ParentId, Session, Threads, Handles, Priority, User, Created, CPUTime, ImagePath,
	};

	void GetDetails(Item const& item) const;
	void ShowProperties(int row) const;
	void UpdateStatusText() const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<Item> m_Items;
	bool m_Threads;
};
