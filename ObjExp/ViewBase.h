#pragma once

#include "Interfaces.h"
#include "ToolbarHelper.h"
#include "ListLayout.h"
#include <ListViewhelper.h>
#include "resource.h"
#include <QuickFindEdit.h>
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
		MESSAGE_HANDLER(WM_DESTROY, OnDestroyBase)
		COMMAND_ID_HANDLER(ID_FILE_SAVE, OnFileSave)
		CHAIN_MSG_MAP(TBase)
	END_MSG_MAP()

	//
	// the list's column widths, order and sort are restored now (if saved) and saved when the view is destroyed;
	// File > Save saves this list. Call after creating the columns
	//
	void InitListLayout(HWND hList, PCWSTR name) {
		m_hLayoutList = hList;
		m_LayoutName = name;
		ListLayout::Restore(static_cast<T*>(this)->m_hWnd, hList, name);
	}

	//
	// a "Quick Find" box in the view's rebar; it sends EN_DELAYCHANGE (WM_COMMAND) to the view while the user types
	//
	void CreateQuickFind(CQuickFindEdit& edit, UINT id = 123) {
		auto pT = static_cast<T*>(this);
		if (pT->m_hWndToolBar == nullptr)
			pT->CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);

		CRect rc(0, 0, 200, 20);
		edit.Create(pT->m_hWnd, rc, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, id);
		edit.SetLimitText(128);
		edit.SetFont(AtlGetDefaultGuiFont());
		edit.SetWatermark(L"Type to filter");
		edit.SetWatermarkIcon(AtlLoadIconImage(IDI_SEARCH, 0, 16, 16));

		WCHAR text[] = L"Quick Find:";
		REBARBANDINFO info = { sizeof(info) };
		info.hwndChild = edit;
		info.fMask = RBBIM_IDEALSIZE | RBBIM_STYLE | RBBIM_TEXT | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE | RBBIM_COLORS;
		info.fStyle = RBBS_CHILDEDGE;
		info.clrBack = ::GetSysColor(COLOR_WINDOW);
		info.clrFore = ::GetSysColor(COLOR_WINDOWTEXT);
		info.lpText = text;
		info.cxIdeal = info.cx = info.cxMinChild = 250;
		info.cyMinChild = 20;
		CReBarCtrl(pT->m_hWndToolBar).InsertBand(-1, &info);
		pT->UpdateLayout();
	}

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
		if (active) {
			static_cast<T*>(this)->UpdateUI(false);
			UI().UIEnable(ID_FILE_SAVE, m_hLayoutList != nullptr);
		}
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
	LRESULT OnFileSave(WORD, WORD, HWND, BOOL& bHandled) {
		if (!m_hLayoutList) {
			bHandled = FALSE;
			return 0;
		}
		auto path = ListViewHelper::PromptForCsvFile(::GetAncestor(m_hLayoutList, GA_ROOT), GetTitle());
		if (!path.IsEmpty() && !ListViewHelper::SaveAsCsv(CListViewCtrl(m_hLayoutList), path))
			AtlMessageBox(static_cast<T*>(this)->m_hWnd, L"Failed to save file.", IDS_TITLE, MB_ICONERROR);
		return 0;
	}

	LRESULT OnDestroyBase(UINT, WPARAM, LPARAM, BOOL& bHandled) {
		if (m_hLayoutList && ::IsWindow(m_hLayoutList))
			ListLayout::Save(m_hLayoutList, m_LayoutName);
		bHandled = FALSE;
		return 0;
	}

	//
	// overridables
	//
	void OnPageActivated(bool activate) {}
	void UpdateUI(bool) {}

	IMainFrame* m_pFrame;
	bool m_IsActive{ true };
	HWND m_hLayoutList{ nullptr };
	CString m_LayoutName;
};
