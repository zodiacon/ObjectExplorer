#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"
#include <mutex>
#include "SearchMatcher.h"

//
// results of searching the names of all handles and of the objects in the object manager namespace
//
class CSearchView :
	public CViewBase<CSearchView>,
	public CVirtualListView<CSearchView> {
public:
	CSearchView(IMainFrame* frame, PCWSTR text, bool matchCase);

	CString GetTitle() const override;
	void Refresh() override;
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnPageActivated(bool active);

	static const UINT WM_SEARCHPROGRESS = WM_APP + 120;

	BEGIN_MSG_MAP(CSearchView)
		MESSAGE_HANDLER(WM_SEARCHPROGRESS, OnSearchProgress)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CViewBase<CSearchView>)
		CHAIN_MSG_MAP(CVirtualListView<CSearchView>)
	END_MSG_MAP()

private:
	struct Result {
		CString Type;
		CString Name;
		CString ProcessName;
		ULONG HandleValue{ 0 };
		ULONG ProcessId{ 0 };
		PVOID Object{ nullptr };
		USHORT TypeIndex{ 0 };
		bool IsHandle{ false };
	};

	enum class ColumnType {
		None,
		Type, Name, ProcessName, PID, Handle, Address,
	};

	void StartSearch();
	void Search();	// worker thread
	void SearchDirectory(CString const& path, std::vector<std::shared_ptr<Result>>& results) const;
	void ShowProperties(int row) const;
	void UpdateStatusText() const;

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnSearchProgress(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	CString m_Text;
	SearchMatcher m_Matcher;
	// shared_ptr so the selection can follow the items when they're re-sorted (SortPreservingSelection)
	std::vector<std::shared_ptr<Result>> m_Results;

	// shared with the worker
	std::mutex m_Lock;
	std::vector<std::shared_ptr<Result>> m_Pending;
	std::atomic<ULONG> m_Searched{ 0 }, m_Total{ 0 };
	std::atomic<bool> m_Cancel{ false };
	std::atomic<bool> m_WorkerRunning{ false };
	bool m_Searching{ false };	// UI thread: until the worker's final message is handled
};
