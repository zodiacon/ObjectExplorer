#pragma once

#include "ViewBase.h"

class CObjectTypesView : public CViewBase<CObjectTypesView> {
public:
	using CViewBase::CViewBase;

	BOOL PreTranslateMessage(MSG* pMsg);

	void OnFinalMessage(HWND /*hWnd*/) override;
	CString GetTitle() const override;

	BEGIN_MSG_MAP(CObjectTypesView)
		MESSAGE_HANDLER(WM_PAINT, OnPaint)
		CHAIN_MSG_MAP(CViewBase<CObjectTypesView>)
	END_MSG_MAP()

	// Handler prototypes (uncomment arguments if needed):
	//	LRESULT MessageHandler(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
	//	LRESULT CommandHandler(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
	//	LRESULT NotifyHandler(int /*idCtrl*/, LPNMHDR /*pnmh*/, BOOL& /*bHandled*/)

	LRESULT OnPaint(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/);
};
