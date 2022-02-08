#pragma once

#include "Interfaces.h"
#include "ToolbarHelper.h"

template<typename T, typename TBase = CFrameWindowImpl<T, CWindow, CControlWinTraits>>
struct CViewBase : IView, TBase, CAutoUpdateUI<T>, CIdleHandler {
	CViewBase(IMainFrame* frame) : m_pFrame(frame) {}

protected:
	BEGIN_MSG_MAP(CViewBase)
		MESSAGE_HANDLER(WM_DESTROY, OnDestroy)
		CHAIN_MSG_MAP(TBase)
	END_MSG_MAP()

	bool ProcessCommand(UINT cmd) {
		LRESULT result;
		return ProcessWindowMessage(static_cast<T*>(this)->m_hWnd, WM_COMMAND, cmd, 0, result, 1);
	}

	BOOL OnIdle() override {
		CAutoUpdateUI<T>::UIUpdateToolBar();
		return FALSE;
	}

	IMainFrame* GetFrame() {
		return m_pFrame;
	}

	HWND GetHwnd() const override {
		return static_cast<T const*>(this)->m_hWnd;
	}

	LRESULT OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& bHandled) {
		auto pT = static_cast<T*>(this);

		bHandled = FALSE;
		if (pT->m_hWndToolBar)
			_Module.GetMessageLoop()->RemoveIdleHandler(this);
		return 0;
	}

	void PageActivated(bool active) override {
		m_IsActive = active;
		static_cast<T*>(this)->OnPageActivated(active);
		if (active)
			static_cast<T*>(this)->UpdateUI(m_pFrame->GetUI());
	}

	HWND CreateAndInitToolBar(const ToolBarButtonInfo* buttons, int count, int size = 24) {
		auto pT = static_cast<T*>(this);

		auto hWndToolBar = ToolbarHelper::CreateAndInitToolBar(pT->m_hWnd, buttons, count, size);

		pT->CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		pT->AddSimpleReBarBand(hWndToolBar);

		pT->UIAddToolBar(hWndToolBar);
		_Module.GetMessageLoop()->AddIdleHandler(this);

		return hWndToolBar;
	}

	//
	// overridables
	//
	void OnPageActivated(bool activate) {}
	void UpdateUI(CUpdateUIBase& ui) {}

private:
	IMainFrame* m_pFrame;
	bool m_IsActive{ false };
};
