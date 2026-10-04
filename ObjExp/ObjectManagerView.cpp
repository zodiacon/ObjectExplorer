#include "pch.h"
#include "ObjectManagerView.h"
#include "resource.h"
#include "SortHelper.h"
#include "NtDll.h"
#include "ResourceManager.h"
#include "IconHelper.h"
//#include "SecurityInfo.h"
#include "ClipboardHelper.h"
#include "ListViewhelper.h"
#include "ObjectHelpers.h"
#include <WTLHelper.h>
#include <unordered_map>
#include <unordered_set>

namespace {
	std::wstring_view View(CString const& s) {
		return std::wstring_view((PCWSTR)s, s.GetLength());
	}
}

CString CObjectManagerView::GetDirectoryPath() const {
	return GetFullItemPath(m_Tree, m_Tree.GetSelectedItem()).Mid(1);
}

void CObjectManagerView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	m_Objects.Sort(0, m_Objects.size(), [&](const auto& data1, const auto& data2) {
		return CompareItems(data1, data2, si->SortColumn, si->SortAscending);
		});
}

CString CObjectManagerView::GetColumnText(HWND, int row, int col) {
	auto& data = m_Objects[row];
	switch (col) {
		case 0:	return data.Name;
		case 1:	return data.Type;
		case 2:	return data.SymbolicLinkTarget;
		case 3:	return data.FullName;
	}
	return L"";
}

int CObjectManagerView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(m_Objects[row].Type);
}

CString CObjectManagerView::GetTitle() const {
	return L"Object Manager";
}

void CObjectManagerView::DoFind(const CString& text, DWORD flags) {
	auto down = flags & FR_DOWN;
	auto dir = down ? 1 : -1;
	auto index = m_List.GetSelectedIndex() + dir;
	auto count = m_List.GetItemCount();
	if (index < 0)
		index = 0;
	else if (index >= count)
		index = count - 1;

	auto matchCase = flags & FR_MATCHCASE;
	CString search(text);
	if (!matchCase)
		search.MakeLower();

	auto start = index;
	do {
		CString value(m_Objects[start].Name);
		if (!matchCase)
			value.MakeLower();
		if (value.Find(search) >= 0)
			break;
		start += dir + count;
		start %= count;
	} while (start != index);

	m_List.SelectItem(start);
	m_List.SetFocus();
}

void CObjectManagerView::UpdateUI(bool force) {
	auto& ui = UI();
	ui.UIEnable(ID_RUN, true);
	CTimerManager::UpdateIntervalUI();
	ui.UISetCheck(ID_PAUSE, false);
	ui.UIEnable(ID_PAUSE, false);
	auto active = m_Splitter.GetActivePane();
	ui.UIEnable(ID_VIEW_PROPERTIES, (active == 1 && m_List.GetSelectedCount() == 1) || active == 0);
	bool copy = (force || ::GetFocus() == m_List) && m_List.GetSelectedCount() > 0;
	ui.UIEnable(ID_EDIT_COPY, copy);
	if (!copy && m_Tree.GetSelectedItem() != nullptr)
		ui.UIEnable(ID_EDIT_COPY, true);
	ui.UIEnable(ID_OBJECTLIST_JUMPTOTARGET, m_List.GetSelectedCount() == 1 && m_Objects[m_List.GetNextItem(-1, LVNI_SELECTED)].Type == L"SymbolicLink");
}

bool CObjectManagerView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row >= 0)
		ShowProperties(row);
	return false;
}

bool CObjectManagerView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	int index = row < 0 ? 3 : 2;
	if (row >= 0) {
		auto& item = m_Objects[row];
		menu.EnableMenuItem(ID_OBJECTLIST_JUMPTOTARGET, item.SymbolicLinkTarget.IsEmpty() ? MF_DISABLED : MF_ENABLED);
	}
	GetFrame()->TrackPopupMenu(menu.GetSubMenu(index), 0, pt.x, pt.y);
	return true;
}

void CObjectManagerView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
	if (m_List.GetSelectedCount() == 1) {
		m_SelectedObjectFullName = m_Objects[m_List.GetNextItem(-1, LVNI_SELECTED)].FullName;
	}
	else {
		m_SelectedObjectFullName.Empty();
	}
}

bool CObjectManagerView::JumpToObject(CString const& fullName) {
	auto dir = fullName.Mid(1);
	auto name = dir.Mid(dir.ReverseFind(L'\\') + 1);
	dir = dir.Left(dir.ReverseFind(L'\\'));
	auto hItem = FindItem(m_Tree, m_Tree.GetRootItem(), dir);
	if (hItem) {
		m_Tree.SelectItem(hItem);
		m_Tree.EnsureVisible(hItem);
		UpdateList(true);
		// index through the vector so the filtered/sorted display order is used
		for (int n = 0; n < (int)m_Objects.size(); n++) {
			if (m_Objects[n].Name == name) {
				m_List.SelectItem(n);
				return true;
			}
		}
	}
	return false;
}

void CObjectManagerView::OnPageActivated(bool active) {
	ActivateTimer(active);
	if (active) {
		UpdateStatusText();
	}
}

void CObjectManagerView::DoTimerUpdate() {
	//
	// merge a fresh enumeration with the current items, keeping the existing order:
	// objects that disappeared are kept (marked Deleted) until their highlight expires,
	// new objects are appended and marked New
	//
	auto tick = ::GetTickCount64();
	auto& old = m_Objects.GetRealAll();
	auto objects = EnumCurrentObjects();

	std::unordered_map<std::wstring_view, size_t> current;
	current.reserve(objects.size());
	for (size_t i = 0; i < objects.size(); i++)
		current.emplace(View(objects[i].FullName), i);

	std::vector<bool> matched(objects.size());
	std::vector<ObjectData> merged;
	merged.reserve(old.size() + objects.size());
	bool changed = false;

	for (auto& prev : old) {
		if (auto it = current.find(View(prev.FullName)); it != current.end()) {
			auto& obj = objects[it->second];
			matched[it->second] = true;
			obj.SymbolicLinkTarget = prev.SymbolicLinkTarget;
			if (prev.State == ObjectState::Deleted) {
				// recreated
				obj.State = ObjectState::New;
				obj.TargetTime = tick + HighlightDuration;
				changed = true;
			}
			else if (prev.State == ObjectState::New && prev.TargetTime <= tick) {
				changed = true;
			}
			else {
				obj.State = prev.State;
				obj.TargetTime = prev.TargetTime;
			}
			merged.push_back(std::move(obj));
		}
		else if (prev.State == ObjectState::Deleted) {
			if (prev.TargetTime > tick)
				merged.push_back(prev);
			else
				changed = true;
		}
		else {
			auto& obj = merged.emplace_back(prev);
			obj.State = ObjectState::Deleted;
			obj.TargetTime = tick + HighlightDuration;
			changed = true;
		}
	}

	for (size_t i = 0; i < objects.size(); i++) {
		if (matched[i])
			continue;
		auto& obj = objects[i];
		if (obj.Type == L"SymbolicLink")
			obj.SymbolicLinkTarget = ObjectManager::GetSymbolicLinkTarget(obj.FullName);
		obj.State = ObjectState::New;
		obj.TargetTime = tick + HighlightDuration;
		merged.push_back(std::move(obj));
		changed = true;
	}

	if (!changed)
		return;

	//
	// selection is by index, so remember selected items by name
	//
	std::unordered_set<std::wstring> selected;
	for (int i = m_List.GetNextItem(-1, LVNI_SELECTED); i >= 0; i = m_List.GetNextItem(i, LVNI_SELECTED))
		selected.insert(std::wstring(m_Objects[i].FullName));
	std::wstring focused;
	if (int i = m_List.GetNextItem(-1, LVNI_FOCUSED); i >= 0)
		focused = m_Objects[i].FullName;

	m_Objects.Set(std::move(merged));
	ApplyFilter(m_FilterText);
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx(static_cast<int>(m_Objects.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);

	if (!selected.empty() || !focused.empty()) {
		m_List.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
		for (int i = 0; i < (int)m_Objects.size(); i++) {
			auto name = View(m_Objects[i].FullName);
			UINT state = 0;
			if (selected.contains(std::wstring(name)))
				state |= LVIS_SELECTED;
			if (name == focused)
				state |= LVIS_FOCUSED;
			if (state)
				m_List.SetItemState(i, state, state);
		}
	}
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

LRESULT CObjectManagerView::OnListCustomDraw(int, LPNMHDR hdr, BOOL& bHandled) {
	if (hdr->hwndFrom != m_List) {
		bHandled = FALSE;
		return 0;
	}

	auto cd = (LPNMLVCUSTOMDRAW)hdr;
	switch (cd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT:
			return CDRF_NOTIFYITEMDRAW;

		case CDDS_ITEMPREPAINT:
			if (auto row = (int)cd->nmcd.dwItemSpec; row < (int)m_Objects.size()) {
				auto state = m_Objects[row].State;
				if (state == ObjectState::New)
					cd->clrTextBk = m_Green;
				else if (state == ObjectState::Deleted)
					cd->clrTextBk = m_Red;
			}
			break;
	}
	return CDRF_DODEFAULT;
}

LRESULT CObjectManagerView::OnUpdateTheme(UINT, WPARAM, LPARAM, BOOL& bHandled) {
	auto dark = WTLHelper::IsDarkMode();
	m_Green = dark ? RGB(0, 128, 0) : RGB(0, 255, 0);
	m_Red = dark ? RGB(128, 0, 0) : RGB(255, 96, 0);
	if (m_List)
		m_List.Invalidate();
	bHandled = FALSE;
	return 0;
}

LRESULT CObjectManagerView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	ToolBarButtonInfo const buttons[] = {
		{ ID_OBJECTLIST_LISTMODE, IDI_LIST, BTNS_CHECK, L"List Mode" },
		{ 0 },
		{ ID_OBJECTLIST_JUMPTOTARGET, IDI_TARGET, BTNS_BUTTON, L"Jump to Target" },
		{ 0 },
		{ ID_OBJECTLIST_SHOWDIRECTORIESINLIST, IDI_DIRECTORY, BTNS_CHECK, L"Show Directories" },
	};
	CToolBarCtrl tb = CreateAndInitToolBar(buttons, _countof(buttons), 16);

	CReBarCtrl rb(m_hWndToolBar);
	CRect rc(0, 0, 120, 20);
	m_QuickFind.Create(m_hWnd, rc, L"", WS_CHILD | WS_VISIBLE | WS_BORDER, 0, 123);
	m_QuickFind.SetLimitText(20);
	m_QuickFind.SetFont((HFONT)::SendMessage(m_hWndToolBar, WM_GETFONT, 0, 0));
	m_QuickFind.SetWatermark(L"Type to filter");
	m_QuickFind.SetWatermarkIcon(AtlLoadIconImage(IDI_SEARCH, 0, 16, 16));

	WCHAR text[] = L"Quick Find:";
	REBARBANDINFO info = { sizeof(info) };
	info.hwndChild = m_QuickFind;
	info.fMask = RBBIM_IDEALSIZE | RBBIM_STYLE | RBBIM_TEXT | RBBIM_CHILD | RBBIM_SIZE | RBBIM_CHILDSIZE | RBBIM_COLORS;
	info.fStyle = RBBS_CHILDEDGE;
	info.clrBack = ::GetSysColor(COLOR_WINDOW);
	info.clrFore = ::GetSysColor(COLOR_WINDOWTEXT);
	info.lpText = text;
	info.cxIdeal = info.cx = info.cxMinChild = 200;
	rb.InsertBand(-1, &info);
	rb.MaximizeBand(1);
	rb.LockBands(true);

	m_hWndClient = m_Splitter.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, WS_EX_CLIENTEDGE);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE |
		WS_CLIPSIBLINGS | LVS_REPORT | LVS_OWNERDATA | LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE |
		WS_CLIPSIBLINGS | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_HASLINES | TVS_SHOWSELALWAYS);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	m_TreeImages.Create(16, 16, ILC_COLOR | ILC_COLOR32, 2, 0);
	ResourceManager::AddIcon(m_TreeImages, IDI_DIRECTORY);
	ResourceManager::AddIcon(m_TreeImages, IDI_FOLDER_CLOSED);
	m_Tree.SetImageList(m_TreeImages, TVSIL_NORMAL);

	m_List.InsertColumn(0, L"Name", LVCFMT_LEFT, 400);
	m_List.InsertColumn(1, L"Type", LVCFMT_LEFT, 150);
	m_List.InsertColumn(2, L"Symbolic Link Target", LVCFMT_LEFT, 600);
	m_List.InsertColumn(3, L"Full Name", LVCFMT_LEFT, 600);

	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	m_Splitter.SetSplitterPosPct(20);
	m_Splitter.SetActivePane(0);
	m_Splitter.UpdateSplitterLayout();

	ObjectManager::EnumTypes();

	SendMessage(::RegisterWindowMessage(L"WTLHelperUpdateTheme"));

	InitTree();

	Run(true);

	return 0;
}

void CObjectManagerView::OnTreeSelChanged(HWND, HTREEITEM hOld, HTREEITEM hNew) {
	ATLASSERT(hNew);
	UpdateList(true);
}

bool CObjectManagerView::OnTreeRightClick(HWND tree, HTREEITEM hItem, POINT const& pt) {
	ATLASSERT(hItem);
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	auto cmd = GetFrame()->TrackPopupMenu(menu.GetSubMenu(1), TPM_RETURNCMD, pt.x, pt.y);
	if (cmd) {
		LRESULT result;
		ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result, 0);
		return true;
	}
	return false;
}

bool CObjectManagerView::OnTreeDoubleClick([[maybe_unused]] HWND tree, HTREEITEM hItem) {
	return ShowProperties(hItem);
}

LRESULT CObjectManagerView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	auto dir = GetDirectoryPath();
	InitTree();
	auto hItem = FindItem(m_Tree, m_Tree.GetRootItem(), dir.Mid(1));
	if (hItem)
		m_Tree.SelectItem(hItem);
	return 0;
}

LRESULT CObjectManagerView::OnEditSecurity(WORD, WORD, HWND, BOOL&) {
	if (m_Splitter.GetActivePane() == 1) {
		int row = m_List.GetNextItem(-1, LVNI_SELECTED);
		if (row >= 0) {
			// copy, as the timer may replace the items while the dialog is open
			auto item = m_Objects[row];
			ObjectHelpers::EditNamespaceObjectSecurity(m_hWnd, item.FullName, item.Type);
		}
	}
	else if (m_Tree.GetSelectedItem()) {
		ObjectHelpers::EditNamespaceObjectSecurity(m_hWnd, GetDirectoryPath(m_Tree.GetSelectedItem()), L"Directory");
	}
	return 0;
}

LRESULT CObjectManagerView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	if (m_Splitter.GetActivePane() == 1) {
		auto text = ListViewHelper::GetSelectedRowsAsString(m_List, L",");
		ClipboardHelper::CopyText(m_hWnd, text);
	}
	else if (m_Splitter.GetActivePane() == 0) {
		auto hSelected = m_Tree.GetSelectedItem();
		if (hSelected) {
			CString text;
			hSelected.GetText(text);
			ClipboardHelper::CopyText(*this, text);
		}
	}
	return 0;
}

void CObjectManagerView::InitTree() {
	m_Tree.SetRedraw(FALSE);
	m_Tree.DeleteAllItems();

	auto root = m_Tree.InsertItem(L"\\", 1, 0, TVI_ROOT, TVI_SORT);
	EnumDirectory(root, L"\\");
	root.SortChildren(TRUE);
	root.Expand(TVE_EXPAND);
	root.Select(TVGN_CARET);

	m_Tree.SetRedraw();
}

std::vector<CObjectManagerView::ObjectData> CObjectManagerView::EnumCurrentObjects() {
	std::vector<ObjectData> objects;
	if (m_ListMode) {
		objects.reserve(512);
		EnumObjectsInDirectory(L"\\", objects);
	}
	else {
		auto path = GetDirectoryPath();
		if (path.IsEmpty())
			path = L"\\";
		objects.reserve(128);
		for (auto const& item : ObjectManager::EnumDirectoryObjects(path)) {
			if (m_ShowDirectories || item.TypeName != L"Directory") {
				ObjectData data;
				data.Name = item.Name.c_str();
				data.Type = item.TypeName.c_str();
				data.FullName = path.Right(1) == L"\\" ? path + data.Name : path + L"\\" + data.Name;
				if (data.FullName.Left(2) == L"\\\\")
					data.FullName.Delete(0);
				objects.push_back(std::move(data));
			}
		}
	}
	return objects;
}

void CObjectManagerView::UpdateList(bool newNode) {
	auto objects = EnumCurrentObjects();
	for (auto& data : objects) {
		if (data.Type == L"SymbolicLink")
			data.SymbolicLinkTarget = ObjectManager::GetSymbolicLinkTarget(data.FullName);
	}
	m_Objects.Set(std::move(objects));
	ApplyFilter(m_FilterText);
	if (newNode) {
		m_List.SetItemCount(static_cast<int>(m_Objects.size()));
		ClearSort();
	}
	else {
		auto si = GetSortInfo(m_List);
		DoSort(si);
		m_List.SetItemCountEx(static_cast<int>(m_Objects.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
	}
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetCountPerPage() + m_List.GetTopIndex());
	UpdateStatusText();
}

bool CObjectManagerView::ShowProperties(int index) const {
	ATLASSERT(index >= 0);
	// copy, as the timer may replace the items while the (modal) properties dialog is open
	auto item = m_Objects[index];
	return ShowProperties(item.FullName, item.Type, item.SymbolicLinkTarget);
}

CString CObjectManagerView::GetDirectoryPath(HTREEITEM hItem) const {
	auto path = GetFullItemPath(m_Tree, hItem).Mid(1);
	// the root's path is empty
	return path.IsEmpty() ? CString(L"\\") : path;
}

bool CObjectManagerView::ShowProperties(HTREEITEM hItem) const {
	return ShowProperties(GetDirectoryPath(hItem), L"Directory");
}

bool CObjectManagerView::ShowProperties(PCWSTR fullName, PCWSTR type, PCWSTR target) const {
	return ObjectHelpers::ShowNamespaceObjectProperties(m_hWnd, fullName, type, target);
}

void CObjectManagerView::EnumDirectory(CTreeItem root, const CString& path) {
	for (auto const& dir : ObjectManager::EnumDirectoryObjects(path)) {
		if (dir.TypeName == L"Directory") {
			auto node = m_Tree.InsertItem(dir.Name.c_str(), 1, 0, root, TVI_LAST);
			EnumDirectory(node, path.Right(1) == L"\\" ? path + dir.Name.c_str() : path + L"\\" + dir.Name.c_str());
		}
	}
}

void CObjectManagerView::EnumObjectsInDirectory(CString const path, std::vector<ObjectData>& objects) {
	for (auto const& dir : ObjectManager::EnumDirectoryObjects(path)) {
		ObjectData data;
		data.FullName = path + L"\\" + dir.Name.c_str();
		if (data.FullName.Left(2) == L"\\\\")
			data.FullName.Delete(0);
		data.Name = dir.Name.c_str();
		data.Type = dir.TypeName.c_str();
		objects.push_back(std::move(data));
		if (dir.TypeName == L"Directory") {
			EnumObjectsInDirectory(path + (path == L"\\" ? L"" : L"\\") + dir.Name.c_str(), objects);
		}
	}
}

bool CObjectManagerView::CompareItems(const ObjectData& data1, const ObjectData& data2, int col, bool asc) {
	switch (col) {
		case 0: return SortHelper::Sort(data1.Name, data2.Name, asc);
		case 1: return SortHelper::Sort(data1.Type, data2.Type, asc);
		case 2: return SortHelper::Sort(data1.SymbolicLinkTarget, data2.SymbolicLinkTarget, asc);
		case 3: return SortHelper::Sort(data1.FullName, data2.FullName, asc);
	}
	return false;
}

LRESULT CObjectManagerView::OnViewProperties(WORD, WORD, HWND, BOOL&) {
	if (m_Splitter.GetActivePane() == 1) {
		int row = m_List.GetNextItem(-1, LVNI_SELECTED);
		if (row >= 0)
			ShowProperties(row);
	}
	else
		ShowProperties(m_Tree.GetSelectedItem());
	return 0;
}

LRESULT CObjectManagerView::OnCopyDirectoryName(WORD, WORD, HWND, BOOL&) {
	ClipboardHelper::CopyText(m_hWnd, GetDirectoryPath());
	return 0;
}

LRESULT CObjectManagerView::OnCopyFullObjectPath(WORD, WORD, HWND, BOOL&) {
	CString text;
	for (auto row : SelectedItemsView(m_List)) {
		auto const& item = m_Objects[row];
		text += item.FullName + L"\n";
	}
	if (!text.IsEmpty()) {
		text = text.Left(text.GetLength() - 1);
		ClipboardHelper::CopyText(m_hWnd, text);
	}

	return 0;
}

LRESULT CObjectManagerView::OnJumpToTarget(WORD, WORD, HWND, BOOL&) {
	auto& item = m_Objects[m_List.GetNextItem(-1, LVNI_SELECTED)];
	ATLASSERT(!item.SymbolicLinkTarget.IsEmpty());
	if(!JumpToObject(item.SymbolicLinkTarget))
		AtlMessageBox(m_hWnd, L"Unable to locate symbolic link target", IDS_TITLE, MB_ICONEXCLAMATION);

	return 0;
}

LRESULT CObjectManagerView::OnQuickTextChanged(WORD, WORD, HWND, BOOL&) {
	m_QuickFind.GetWindowText(m_FilterText);
	ApplyFilter(m_FilterText);
	// filtering rebuilds the rows in their natural order
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Objects.size(), LVSICF_NOSCROLL);
	return 0;
}

void CObjectManagerView::ApplyFilter(PCWSTR filter) {
	if (filter == nullptr || *filter == 0)
		m_Objects.Filter(nullptr);
	else {
		CString text(filter);
		text.MakeLower();
		m_Objects.Filter([=](auto item, auto) {
			CString search(item.Name);
			search.MakeLower();
			if (search.Find(text) >= 0)
				return true;
			search = item.Type;
			search.MakeLower();
			if (search.Find(text) >= 0)
				return true;
			search = item.SymbolicLinkTarget;
			search.MakeLower();
			return search.Find(text) >= 0;
			});
	}
	UpdateStatusText();
}
void CObjectManagerView::UpdateStatusText() {
	if (IsActive()) {
		CString text;
		text.Format(L"Items: %u\n", (ULONG)m_Objects.size());
		GetFrame()->SetStatusText(7, text);
	}
}

LRESULT CObjectManagerView::OnQuickFind(WORD, WORD, HWND, BOOL&) {
	m_QuickFind.SetFocus();
	return 0;
}

LRESULT CObjectManagerView::OnShowDirectories(WORD, WORD, HWND, BOOL&) {
	m_ShowDirectories = !m_ShowDirectories;
	UI().UISetCheck(ID_OBJECTLIST_SHOWDIRECTORIESINLIST, m_ShowDirectories);
	UpdateList(false);

	return 0;
}

LRESULT CObjectManagerView::OnSwitchToListMode(WORD, WORD, HWND, BOOL&) {
	m_ListMode = !m_ListMode;
	UI().UISetCheck(ID_OBJECTLIST_LISTMODE, m_ListMode);
	CString selected(m_SelectedObjectFullName);
	m_Splitter.SetSinglePaneMode(m_ListMode ? 1 : SPLIT_PANE_NONE);
	UpdateList(true);
	if (!selected.IsEmpty()) {
		if (m_ListMode) {
			//
			// find the item in the list
			//
			for (int n = 0; n < (int)m_Objects.size(); n++) {
				if (m_Objects[n].FullName == selected) {
					m_List.SelectItem(n);
					break;
				}
			}
		}
		else {
			JumpToObject(selected);
		}
	}
	return 0;
}

