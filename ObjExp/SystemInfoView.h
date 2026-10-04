#pragma once

#include "resource.h"
#include "ViewBase.h"
#include "VirtualListView.h"
#include "TimerManager.h"

class CSystemInfoView :
	public CViewBase<CSystemInfoView>,
	public CTimerManager<CSystemInfoView>,
	public CVirtualListView<CSystemInfoView> {
public:
	using CViewBase::CViewBase;

	CString GetTitle() const override;
	void Refresh() override;
	CString GetColumnText(HWND, int row, int col) const;
	void UpdateUI(bool force = false);
	void OnPageActivated(bool active);
	void DoTimerUpdate();

	BEGIN_MSG_MAP(CSystemInfoView)
		COMMAND_ID_HANDLER(ID_EDIT_COPY, OnEditCopy)
		COMMAND_ID_HANDLER(ID_VIEW_REFRESH, OnViewRefresh)
		MESSAGE_HANDLER(WM_CREATE, OnCreate)
		CHAIN_MSG_MAP(CTimerManager<CSystemInfoView>)
		CHAIN_MSG_MAP(CViewBase<CSystemInfoView>)
		CHAIN_MSG_MAP(CVirtualListView<CSystemInfoView>)
		CHAIN_MSG_MAP_ALT(CTimerManager<CSystemInfoView>, 1)
	END_MSG_MAP()

private:
	LRESULT OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
	LRESULT OnEditCopy(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const;
	LRESULT OnViewRefresh(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/);

	CListViewCtrl m_List;
	std::vector<std::pair<CString, CString>> m_Items;
};
