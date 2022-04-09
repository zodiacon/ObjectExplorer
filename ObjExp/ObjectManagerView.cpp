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

CString CObjectManagerView::GetDirectoryPath() const {
	return GetFullItemPath(m_Tree, m_Tree.GetSelectedItem()).Mid(1);
}

void CObjectManagerView::OnFinalMessage(HWND) {
	delete this;
}

void CObjectManagerView::DoSort(const SortInfo* si) {
	if (si == nullptr)
		return;

	std::sort(m_Objects.begin(), m_Objects.end(), [=](const auto& data1, const auto& data2) {
		return CompareItems(data1, data2, si->SortColumn, si->SortAscending);
		});
}

CString CObjectManagerView::GetColumnText(HWND, int row, int col) {
	auto& data = m_Objects[row];
	switch (col) {
		case 0:	return data.Name;
		case 1:	return data.Type;
		case 2: 
			if(data.Type == L"SymbolicLink" && data.SymbolicLinkTarget.IsEmpty())
				data.SymbolicLinkTarget = ObjectManager::GetSymbolicLinkTarget(data.FullName);
			return data.SymbolicLinkTarget;
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

void CObjectManagerView::UpdateUI(CUpdateUIBase& ui, bool force) {
	ui.UISetCheck(ID_RUN, false);
	ui.UIEnable(ID_RUN, false);
	ui.UISetCheck(ID_PAUSE, false);
	ui.UIEnable(ID_PAUSE, false);
	auto active = m_Splitter.GetActivePane();
	ui.UIEnable(ID_VIEW_PROPERTIES, (active == 1 && m_List.GetSelectedCount() == 1) || active == 0);
	bool copy = (force || ::GetFocus() == m_List) && m_List.GetSelectedCount() > 0;
	ui.UIEnable(ID_EDIT_COPY, copy);
	if (!copy && m_Tree.GetSelectedItem() != nullptr)
		ui.UIEnable(ID_EDIT_COPY, true);
}

bool CObjectManagerView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	ShowProperties(row);
	return false;
}

bool CObjectManagerView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	if (row >= 0) {
		CMenu menu;
		menu.LoadMenu(IDR_CONTEXT);
		auto& item = m_Objects[row];
		menu.EnableMenuItem(ID_OBJECTLIST_JUMPTOTARGET, item.SymbolicLinkTarget.IsEmpty() ? MF_DISABLED : MF_ENABLED);
		auto cmd = GetFrame()->TrackPopupMenu(menu.GetSubMenu(2), TPM_RETURNCMD, pt.x, pt.y);
		if (cmd) {
			LRESULT result;
			ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result, 1);
			return true;
		}
	}
	return false;
}

LRESULT CObjectManagerView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_Splitter.Create(*this, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
	m_List.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | 
		WS_CLIPSIBLINGS | LVS_REPORT | LVS_OWNERDATA | LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS);
	m_Tree.Create(m_Splitter, rcDefault, nullptr, WS_CHILD | WS_VISIBLE |
		WS_CLIPSIBLINGS | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_HASLINES | TVS_SHOWSELALWAYS);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);
	CImageList images;
	images.Create(16, 16, ILC_COLOR | ILC_COLOR32, 2, 0);
	images.AddIcon(AtlLoadIconImage(IDI_DIRECTORY, 0, 16, 16));
	images.AddIcon(AtlLoadIconImage(IDI_FOLDER_CLOSED, 0, 16, 16));
	m_Tree.SetImageList(images, TVSIL_NORMAL);

	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP);
	m_List.InsertColumn(0, L"Name", LVCFMT_LEFT, 400);
	m_List.InsertColumn(1, L"Type", LVCFMT_LEFT, 150);
	m_List.InsertColumn(2, L"Symbolic Link Target", LVCFMT_LEFT, 600);

	m_Splitter.SetSplitterExtendedStyle(SPLIT_FLATBAR | SPLIT_PROPORTIONAL);
	m_Splitter.SetSplitterPanes(m_Tree, m_List);
	m_Splitter.SetSplitterPosPct(20);
	m_Splitter.SetActivePane(0);
	m_Splitter.UpdateSplitterLayout();

	m_mgr.EnumTypes();

	InitTree();

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
		ProcessWindowMessage(m_hWnd, WM_COMMAND, cmd, 0, result, 1);
		return true;
	}
	return false;
}

bool CObjectManagerView::OnTreeDoubleClick([[maybe_unused]]HWND tree, HTREEITEM hItem) {
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
	auto index = m_List.GetSelectedIndex();
	ATLASSERT(index >= 0);
	auto& item = m_Objects[index];

	return 0;
}

LRESULT CObjectManagerView::OnEditCopy(WORD, WORD, HWND, BOOL&) {
	if(m_Splitter.GetActivePane() == 1) {
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

LRESULT CObjectManagerView::OnListStateChanged(int, LPNMHDR, BOOL&) {
	UpdateUI(GetFrame()->GetUI(), true);
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

void CObjectManagerView::UpdateList(bool newNode) {
	auto path = GetDirectoryPath();
	if (path.IsEmpty())
		path = L"\\";
	m_Objects.clear();
	m_Objects.reserve(128);
	for (auto& item : ObjectManager::EnumDirectoryObjects(path)) {
		if (item.TypeName != L"Directory") {
			ObjectData data;
			data.Name = item.Name.c_str();
			data.Type = item.TypeName.c_str();
			data.FullName = path.Right(1) == L"\\" ? path + data.Name : path + L"\\" + data.Name;
			m_Objects.push_back(std::move(data));
		}
	}
	if (newNode) {
		m_List.SetItemCount(static_cast<int>(m_Objects.size()));
		ClearSort();
	}
	else {
		auto si = GetSortInfo(m_List);
		DoSort(si);
		m_List.SetItemCountEx(static_cast<int>(m_Objects.size()), LVSICF_NOINVALIDATEALL | LVSICF_NOSCROLL);
		m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetCountPerPage() + m_List.GetTopIndex());
	}
}

bool CObjectManagerView::ShowProperties(int index) const {
	ATLASSERT(index >= 0);
	auto& item = m_Objects[index];
	return ShowProperties(item.FullName, item.Type, item.SymbolicLinkTarget);
}

bool CObjectManagerView::ShowProperties(HTREEITEM hItem) const {
	auto path = GetFullItemPath(m_Tree, hItem);
	return ShowProperties(path.Mid(1), L"Directory");
}

bool CObjectManagerView::ShowProperties(PCWSTR fullName, PCWSTR type, PCWSTR target) const {
	HANDLE hObject;
	auto status = ObjectManager::OpenObject(fullName, type, hObject);
	if (hObject) {
		ObjectHelpers::ShowObjectProperties(hObject, type, fullName, target);
		::CloseHandle(hObject);
		return true;
	}
	AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
	return false;
}

void CObjectManagerView::EnumDirectory(CTreeItem root, const CString& path) {
	for (auto const& dir : ObjectManager::EnumDirectoryObjects(path)) {
		if (dir.TypeName == L"Directory") {
			auto node = m_Tree.InsertItem(dir.Name.c_str(), 1, 0, root, TVI_LAST);
			EnumDirectory(node, path.Right(1) == L"\\" ? path + dir.Name.c_str() : path + L"\\" + dir.Name.c_str());
		}
	}
}

bool CObjectManagerView::CompareItems(const ObjectData& data1, const ObjectData& data2, int col, bool asc) {
	switch (col) {
		case 0: return SortHelper::Sort(data1.Name, data2.Name, asc);
		case 1: return SortHelper::Sort(data1.Type, data2.Type, asc);
		case 2: return SortHelper::Sort(data1.SymbolicLinkTarget, data2.SymbolicLinkTarget, asc);
	}
	return false;
}

LRESULT CObjectManagerView::OnViewProperties(WORD, WORD, HWND, BOOL&) {
	if (m_Splitter.GetActivePane() == 1)
		ShowProperties(m_List.GetSelectionMark());
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
	auto& item = m_Objects[m_List.GetSelectionMark()];
	ATLASSERT(!item.SymbolicLinkTarget.IsEmpty());
	auto dir = item.SymbolicLinkTarget.Mid(1);
	auto name = dir.Mid(dir.ReverseFind(L'\\') + 1);
	dir = dir.Left(dir.ReverseFind(L'\\'));
	auto hItem = FindItem(m_Tree, m_Tree.GetRootItem(), dir);
	if (hItem) {
		m_Tree.SelectItem(hItem);
		UpdateList(true);
		int n = m_List.FindItem(name, false);
		if (n >= 0) {
			m_List.SetItemState(n, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			m_List.EnsureVisible(n, FALSE);
			return 0;
		}
	}
	AtlMessageBox(m_hWnd, L"Unable to locate symbolic link target", IDS_TITLE, MB_ICONEXCLAMATION);

	return 0;
}
