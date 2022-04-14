#pragma once

#include "VirtualListView.h"
#include "DialogHelper.h"
#include "SortedFilteredVector.h"
#include "QuickFindEdit.h"

class CProcessSelectorDlg : 
	public CDialogImpl<CProcessSelectorDlg>,
	public CDialogHelper<CProcessSelectorDlg>,
	public CDynamicDialogLayout<CProcessSelectorDlg>,
	public CVirtualListView<CProcessSelectorDlg> {
public:
	enum { IDD = IDD_PROCESSCHOOSER };

	DWORD GetSelectedProcess() const;

	CString GetColumnText(HWND, int row, int col) const;
	int GetRowImage(HWND, int row, int col) const;
	void DoSort(SortInfo const* si);
	void OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState);
	bool OnDoubleClickList(HWND, int row, int col, CPoint const&);

	BEGIN_MSG_MAP(CProcessSelectorDlg)
		MESSAGE_HANDLER(WM_HOTKEY, OnHotKey)
		COMMAND_CODE_HANDLER(EN_DELAYCHANGE, OnFilterChanged)
		COMMAND_ID_HANDLER(IDOK, OnCloseCmd)
		COMMAND_ID_HANDLER(IDC_REFRESH, OnRefresh)
		COMMAND_ID_HANDLER(IDCANCEL, OnCloseCmd)
		MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
		CHAIN_MSG_MAP(CVirtualListView<CProcessSelectorDlg>)
		CHAIN_MSG_MAP(CDynamicDialogLayout<CProcessSelectorDlg>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnCloseCmd(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnHotKey(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnFilterChanged(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);
	LRESULT OnRefresh(WORD /*wNotifyCode*/, WORD id, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

private:
	void ApplyFilter(PCWSTR text);

	struct ProcessInfo {
		DWORD Id;
		std::wstring Name;
		std::wstring UserName;
		DWORD Session;
		mutable int Image{ -1 };
	};

	void InitProcesses();

private:
	SortedFilteredVector<ProcessInfo> m_Processes;
	mutable CImageList m_Icons;
	mutable std::unordered_map<std::wstring, int> m_IconsMap;
	CListViewCtrl m_List;
	CQuickFindEdit m_QuickFind;
	DWORD m_SelectedPid{ 0 };
};

