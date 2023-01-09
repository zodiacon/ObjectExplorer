#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"
#include "TimerManager.h"
#include "ProcessHandleTracker.h"
#include "SortedFilteredVector.h"

class CHandlesView :
	public CViewBase<CHandlesView>,
	public CCustomDraw<CHandlesView>,
	public CTimerManager<CHandlesView>,
	public CVirtualListView<CHandlesView> {
public:
	explicit CHandlesView(IMainFrame* frame, DWORD pid = 0, PCWSTR type = nullptr);

	CString GetTitle() const override;
	void Refresh();
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnPageActivated(bool active);
	void DoTimerUpdate();
	int GetSaveColumnRange(HWND, int& start) const;

	DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);

	const UINT WM_CONTINUEUPDATE = WM_APP + 111;

	BEGIN_MSG_MAP(CHandlesView)
		MESSAGE_HANDLER(WM_CONTINUEUPDATE, OnContinueUpdate)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_HANDLELIST_CLOSE, OnCloseHandles)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(CTimerManager<CHandlesView>)
		CHAIN_MSG_MAP(CCustomDraw<CHandlesView>)
		CHAIN_MSG_MAP(CViewBase<CHandlesView>)
		CHAIN_MSG_MAP(CVirtualListView<CHandlesView>)
		CHAIN_MSG_MAP_ALT(CTimerManager<CHandlesView>, 1)
	END_MSG_MAP()

private:
	void ShowObjectProperties(int row) const;
	void UpdateStatusText() const;
	void DoTimerWorkAsync();

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnContinueUpdate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnPauseResume(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnCloseHandles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	struct HandleInfoEx : HandleInfo {
		CString ProcessName;
		CString Type;
		DWORD64 TargetTime;
		bool NameChecked : 1{ false };
		bool NewHandle : 1{false };
		bool ClosedHandle : 1 { false};
	};

	enum class ColumnType {
		None,
		Type, Handle, Name, Address, Attributes, Access, DecodedAccess, ProcessName, PID,
	};

	CListViewCtrl m_List;
	ProcessHandlesTracker<HandleInfoEx> m_Tracker;
	SortedFilteredVector<std::shared_ptr<HandleInfoEx>> m_Handles;
	std::vector<std::shared_ptr<HandleInfoEx>> m_NewHandles, m_TempHandles;
	DWORD m_Pid;
	CString m_TypeName;
	wil::unique_handle m_hProcess;
	std::atomic<bool> m_UpdateInProgress{ false };
	bool m_UpdateProcNames : 1{ false }, m_UpdateObjectNames : 1{ false };
};
