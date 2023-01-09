#pragma once

#include "Interfaces.h"
#include "ToolbarHelper.h"

template<typename T, typename TBase = CFrameWindowImpl<T, CWindow, CControlWinTraits>>
class CViewBase : public IView, public TBase {
public:
	explicit CViewBase(IMainFrame* frame) : m_pFrame(frame) {}

protected:
	BEGIN_MSG_MAP(CViewBase)
		CHAIN_MSG_MAP(TBase)
	END_MSG_MAP()

	void OnFinalMessage(HWND /*hWnd*/) override {
		delete this;
	}

	bool ProcessCommand(UINT cmd) {
		LRESULT result;
		return ProcessWindowMessage(static_cast<T*>(this)->m_hWnd, WM_COMMAND, LOWORD(cmd), 0, result, 0);
	}

	CUpdateUIBase& UI() {
		return m_pFrame->GetUI();
	}

	IMainFrame* GetFrame() const {
		return m_pFrame;
	}

	HWND GetHwnd() const override {
		return static_cast<T const*>(this)->m_hWnd;
	}

	bool IsActive() const {
		return m_IsActive;
	}

	void PageActivated(bool active) override {
		m_IsActive = active;
		static_cast<T*>(this)->OnPageActivated(active);
		if (active)
			static_cast<T*>(this)->UpdateUI(false);
	}

	HWND CreateAndInitToolBar(const ToolBarButtonInfo* buttons, int count, int size = 24) {
		auto pT = static_cast<T*>(this);
		auto hWndToolBar = ToolbarHelper::CreateAndInitToolBar(pT->m_hWnd, buttons, count, size);
		if (pT->m_hWndToolBar == nullptr) {
			pT->CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
		}
		pT->AddSimpleReBarBand(hWndToolBar);

		GetFrame()->AddToolBar(hWndToolBar);

		return hWndToolBar;
	}

private:
	//
	// overridables
	//
	void OnPageActivated(bool activate) {}
	void UpdateUI(bool) {}

	IMainFrame* m_pFrame;
	bool m_IsActive{ true };
};
