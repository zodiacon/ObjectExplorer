#pragma once

#include "Interfaces.h"
#include "ToolbarHelper.h"
#include <unordered_set>

//
// runs a sort of a virtual list's items, then reselects the rows by item identity,
// as selection in an owner data list is by index. Items must be hashable (e.g. shared_ptr).
//
template<typename TItems, typename TSort>
void SortPreservingSelection(CListViewCtrl& list, TItems const& items, TSort&& sort) {
	using Item = std::decay_t<decltype(items[0])>;
	std::unordered_set<Item> selected;
	for (int i = list.GetNextItem(-1, LVNI_SELECTED); i >= 0 && i < (int)items.size(); i = list.GetNextItem(i, LVNI_SELECTED))
		selected.insert(items[i]);
	int focused = list.GetNextItem(-1, LVNI_FOCUSED);
	Item focusedItem = focused >= 0 && focused < (int)items.size() ? items[focused] : Item{};

	sort();

	if (selected.empty() && !focusedItem)
		return;

	list.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
	for (int i = 0; i < (int)items.size(); i++) {
		auto const& item = items[i];
		UINT state = 0;
		if (selected.contains(item))
			state |= LVIS_SELECTED;
		if (focusedItem && item == focusedItem)
			state |= LVIS_FOCUSED;
		if (state)
			list.SetItemState(i, state, state);
	}
}

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
