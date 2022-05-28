#pragma once

#include "ViewBase.h"
#include <VirtualListView.h>
#include "resource.h"

class CZombieProcessesView : 
	public CViewBase<CZombieProcessesView>,
	public CVirtualListView<CZombieProcessesView> {
public:
	CZombieProcessesView(IMainFrame* frame, bool processes) : CViewBase(frame), m_Processes(processes) {}

	CString GetTitle() const override;
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	int GetSaveColumnRange(int& start) const;
	bool IsSortable(HWND, int col) const;
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	bool OnDoubleClickList(HWND, int row, int col, CPoint const&);

	void UpdateUI(bool active = true);

	BEGIN_MSG_MAP(CZombieProcessesView)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_PROPERTIES, OnViewProperties)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CZombieProcessesView>)
		CHAIN_MSG_MAP(CViewBase<CZombieProcessesView>)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Id, Name, Handles, CreateTime, ExitTime, KernelTime, UserTime, CPUTime, ExitCode, Details,
		Pid,
	};

	struct HandleEntry {
		ULONG Handle;
		DWORD Pid;
	};
	struct ZombieProcessOrThread {
		DWORD Id, Pid;	// PID valid for thread objects
		DWORD ExitCode{ 0 };
		std::wstring Name, FullPath;
		std::vector<HandleEntry> Handles;
		DWORD64 CreateTime, ExitTime, KernelTime, UserTime;
	};

	void RefreshProcesses();
	void RefreshThreads();
	void Refresh();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<ZombieProcessOrThread> m_Items;
	bool m_Processes;
};

