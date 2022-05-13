#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"
#include "TimerManager.h"
#include "SortedFilteredVector.h"
#include "ProcessObjectTracker.h"

class CObjectsView :
	public CViewBase<CObjectsView>,
//	public CCustomDraw<CObjectsView>,
//	public CTimerManager<CObjectsView>,
	public CVirtualListView<CObjectsView> {
public:
	explicit CObjectsView(IMainFrame* frame, PCWSTR type = nullptr);

	CString GetTitle() const override;
	void Refresh();
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	//void UpdateUI(bool force = false);
	bool OnDoubleClickList(HWND, int row, int col, POINT const& pt) const;
	//bool OnRightClickList(HWND, int row, int col, POINT const& pt);
	//void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	void OnPageActivated(bool active);
	//void DoTimerUpdate();

	//DWORD OnPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);
	//DWORD OnItemPrePaint(int /*idCtrl*/, LPNMCUSTOMDRAW /*lpNMCustomDraw*/);

	const UINT WM_CONTINUEUPDATE = WM_APP + 111;

	BEGIN_MSG_MAP(CObjectsView)
//		MESSAGE_HANDLER(WM_CONTINUEUPDATE, OnContinueUpdate)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
//		CHAIN_MSG_MAP(CTimerManager<CObjectsView>)
//		CHAIN_MSG_MAP(CCustomDraw<CObjectsView>)
		CHAIN_MSG_MAP(CViewBase<CObjectsView>)
		CHAIN_MSG_MAP(CVirtualListView<CObjectsView>)
//		CHAIN_MSG_MAP_ALT(CTimerManager<CObjectsView>, 1)
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

	struct ObjectInfoEx : ObjectInfo {
		CString Type;
		DWORD64 TargetTime;
		bool NameChecked : 1 { false };
		bool NewObject : 1 {false };
		bool DeletedObject : 1 { false};
	};

	enum class ColumnType {
		None,
		Type, Handles, RefCount, Name, Address,
	};

	CListViewCtrl m_List;
	SortedFilteredVector<std::shared_ptr<ObjectInfoEx>> m_Objects;
	ProcessObjectsTracker<ObjectInfoEx> m_Tracker;
	std::vector<std::shared_ptr<ObjectInfoEx>> m_NewObjects, m_TempObjects;
	DWORD m_Pid;
	CString m_TypeName;
	wil::unique_handle m_hProcess;
	std::atomic<bool> m_UpdateInProgress{ false };
	bool m_UpdateProcNames{ false }, m_UpdateObjectNames{ false };
};
