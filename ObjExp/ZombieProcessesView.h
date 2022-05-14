#pragma once

#include "ViewBase.h"
#include <VirtualListView.h>

class CZombieProcessesView : 
	public CViewBase<CZombieProcessesView>,
	public CVirtualListView<CZombieProcessesView> {
public:
	using CViewBase::CViewBase;

	CString GetTitle() const override;
	void DoSort(SortInfo const* si);
	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	int GetSaveColumnRange(int& start) const;
	bool IsSortable(HWND, int col) const;

	BEGIN_MSG_MAP(CZombieProcessesView)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CVirtualListView<CZombieProcessesView>)
		CHAIN_MSG_MAP(CViewBase<CZombieProcessesView>)
	END_MSG_MAP()

private:
	enum class ColumnType {
		Pid, Name, Handles, CreateTime, ExitTime, KernelTime, UserTime, CPUTime, ExitCode, Details,
	};

	struct HandleEntry {
		ULONG Handle;
		DWORD Pid;
	};
	struct ZombieProcess {
		DWORD Pid;
		DWORD ExitCode{ 0 };
		std::wstring Name, FullPath;
		std::vector<HandleEntry> Handles;
		DWORD64 CreateTime, ExitTime, KernelTime, UserTime;
	};

	void Refresh();

	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewProperties(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<ZombieProcess> m_Items;
};

