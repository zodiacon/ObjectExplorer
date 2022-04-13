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
	using CViewBase::CViewBase;

	explicit CHandlesView(IMainFrame* frame, DWORD pid = 0, PCWSTR processName = nullptr);

	void OnFinalMessage(HWND /*hWnd*/) override;
	CString GetTitle() const override;
	void Refresh();
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;

	BEGIN_MSG_MAP(CHandlesView)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CTimerManager<CHandlesView>)
		CHAIN_MSG_MAP(CCustomDraw<CHandlesView>)
		CHAIN_MSG_MAP(CViewBase<CHandlesView>)
		CHAIN_MSG_MAP(CVirtualListView<CHandlesView>)
		CHAIN_MSG_MAP_ALT(CTimerManager<CHandlesView>, 1)
	END_MSG_MAP()

private:
	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnPauseResume(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	struct HandleInfoEx : HandleInfo {
		CString ProcessName;
		CString Type;
		bool NameChecked{ false };
	};

	enum class ColumnType {
		None,
		Type, Handle, Name, Address, Attributes, Access, DecodedAccess, ProcessName, PID,
	};

	CListViewCtrl m_List;
	ProcessHandlesTracker<HandleInfoEx> m_Tracker;
	SortedFilteredVector<std::shared_ptr<HandleInfoEx>> m_Handles;
	DWORD m_Pid;
	CString m_ProcessName;
	bool m_UpdateProcNames{ false }, m_UpdateObjectNames{ false };
};
