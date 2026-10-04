#include "pch.h"
#include "SearchView.h"
#include "ObjectManager.h"
#include "ObjectHelpers.h"
#include "ProcessHelper.h"
#include "ResourceManager.h"
#include "SortHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"

CSearchView::CSearchView(IMainFrame* frame, PCWSTR text, bool matchCase)
	: CViewBase(frame), m_Text(text), m_LowerText(text), m_MatchCase(matchCase) {
	m_LowerText.MakeLower();
}

CString CSearchView::GetTitle() const {
	return L"Find: " + m_Text;
}

void CSearchView::Refresh() {
	if (!m_Searching)
		StartSearch();
}

void CSearchView::StartSearch() {
	m_Results.clear();
	m_List.SetItemCount(0);
	{
		std::lock_guard lock(m_Lock);
		m_Pending.clear();
	}
	m_Searched = m_Total = 0;
	m_Cancel = false;
	m_Searching = true;
	m_WorkerRunning = true;
	if (!::TrySubmitThreadpoolCallback([](auto, auto param) {
		((CSearchView*)param)->Search();
		}, this, nullptr)) {
		m_WorkerRunning = false;
		m_Searching = false;
	}
	UpdateStatusText();
}

bool CSearchView::Matches(CString const& name) const {
	if (m_MatchCase)
		return name.Find(m_Text) >= 0;
	CString lower(name);
	return lower.MakeLower().Find(m_LowerText) >= 0;
}

void CSearchView::SearchDirectory(CString const& path, std::vector<std::shared_ptr<Result>>& results) const {
	for (auto const& item : ObjectManager::EnumDirectoryObjects(path)) {
		if (m_Cancel)
			return;
		auto fullName = (path == L"\\" ? path : path + L"\\") + item.Name.c_str();
		if (Matches(fullName)) {
			auto r = std::make_shared<Result>();
			r->Type = item.TypeName.c_str();
			r->Name = fullName;
			results.push_back(std::move(r));
		}
		if (item.TypeName == L"Directory")
			SearchDirectory(fullName, results);
	}
}

void CSearchView::Search() {
	std::vector<std::shared_ptr<Result>> results;
	auto lastFlush = ::GetTickCount64();
	auto flush = [&] {
		{
			std::lock_guard lock(m_Lock);
			for (auto& r : results)
				m_Pending.push_back(std::move(r));
		}
		results.clear();
		PostMessage(WM_SEARCHPROGRESS, 0);
		lastFlush = ::GetTickCount64();
	};

	//
	// the object manager namespace (includes named objects nobody has a handle to)
	//
	SearchDirectory(L"\\", results);
	flush();

	//
	// the names of all handles
	//
	auto handles = ObjectManager::EnumHandles2<>(nullptr, 0, false, true);
	m_Total = (ULONG)handles.size();
	std::unordered_map<ULONG, CString> processNames;
	ULONG count = 0;
	for (auto& hi : handles) {
		if (m_Cancel)
			break;
		m_Searched = ++count;
		if (::GetTickCount64() - lastFlush > 500)
			flush();

		if (!ObjectHelpers::IsNamedObjectType(hi->ObjectTypeIndex))
			continue;
		auto name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId, hi->ObjectTypeIndex, hi->Object);
		if (name.IsEmpty() || !Matches(name))
			continue;

		auto r = std::make_shared<Result>();
		r->IsHandle = true;
		r->Name = name;
		r->Type = ObjectManager::GetType(hi->ObjectTypeIndex)->TypeName;
		r->TypeIndex = hi->ObjectTypeIndex;
		r->HandleValue = hi->HandleValue;
		r->ProcessId = hi->ProcessId;
		r->Object = hi->Object;
		auto& processName = processNames[hi->ProcessId];
		if (processName.IsEmpty())
			processName = ProcessHelper::GetProcessName(hi->ProcessId);
		r->ProcessName = processName;
		results.push_back(std::move(r));
	}

	{
		std::lock_guard lock(m_Lock);
		for (auto& r : results)
			m_Pending.push_back(std::move(r));
	}
	PostMessage(WM_SEARCHPROGRESS, 1);
	// must be last: once cleared, the view may be destroyed
	m_WorkerRunning = false;
}

LRESULT CSearchView::OnSearchProgress(UINT, WPARAM done, LPARAM, BOOL&) {
	std::vector<std::shared_ptr<Result>> pending;
	{
		std::lock_guard lock(m_Lock);
		pending.swap(m_Pending);
	}
	if (!pending.empty()) {
		SortPreservingSelection(m_List, m_Results, [&] {
			m_Results.insert(m_Results.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(pending.end()));
			DoSort(GetSortInfo(m_List));
			m_List.SetItemCountEx((int)m_Results.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
			});
		m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	}
	if (done)
		m_Searching = false;
	UpdateStatusText();
	return 0;
}

void CSearchView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	std::sort(m_Results.begin(), m_Results.end(), [&](auto const& r1, auto const& r2) {
		switch (col) {
			case ColumnType::Type: return SortHelper::Sort(r1->Type, r2->Type, asc);
			case ColumnType::Name: return SortHelper::Sort(r1->Name, r2->Name, asc);
			case ColumnType::ProcessName: return SortHelper::Sort(r1->ProcessName, r2->ProcessName, asc);
			case ColumnType::PID: return SortHelper::Sort(r1->ProcessId, r2->ProcessId, asc);
			case ColumnType::Handle: return SortHelper::Sort(r1->HandleValue, r2->HandleValue, asc);
			case ColumnType::Address: return SortHelper::Sort(r1->Object, r2->Object, asc);
		}
		return false;
		});
}

CString CSearchView::GetColumnText(HWND, int row, int col) const {
	auto& r = m_Results[row];
	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Type: return r->Type;
		case ColumnType::Name: return r->Name;
		case ColumnType::ProcessName: return r->IsHandle ? r->ProcessName : CString(L"<namespace>");
		case ColumnType::PID: return r->IsHandle ? std::to_wstring(r->ProcessId).c_str() : L"";
		case ColumnType::Handle: return r->IsHandle ? std::format(L"0x{:X}", r->HandleValue).c_str() : L"";
		case ColumnType::Address: return r->Object ? std::format(L"0x{:X}", (ULONG_PTR)r->Object).c_str() : L"";
	}
	return L"";
}

int CSearchView::GetRowImage(HWND, int row, int col) const {
	auto& r = m_Results[row];
	auto& rm = ResourceManager::Get();
	return r->IsHandle ? rm.GetTypeImage(r->TypeIndex) : rm.GetTypeImage(r->Type);
}

void CSearchView::UpdateUI(bool force) {
	auto& ui = UI();
	int selected = m_List.GetSelectedCount();
	ui.UIEnable(ID_VIEW_PROPERTIES, selected == 1);
	ui.UIEnable(ID_EDIT_COPY, selected > 0);
}

bool CSearchView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row >= 0)
		ShowProperties(row);
	return true;
}

void CSearchView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CSearchView::OnPageActivated(bool active) {
	if (active) {
		UpdateUI();
		UpdateStatusText();
	}
}

void CSearchView::ShowProperties(int row) const {
	// copy, as more results may arrive while the properties dialog is open
	auto r = m_Results[row];
	if (!r->IsHandle) {
		CString target;
		if (r->Type == L"SymbolicLink")
			target = ObjectManager::GetSymbolicLinkTarget(r->Name);
		ObjectHelpers::ShowNamespaceObjectProperties(m_hWnd, r->Name, r->Type, target.IsEmpty() ? nullptr : (PCWSTR)target);
		return;
	}
	auto hObject = ObjectManager::DupHandle(ULongToHandle(r->HandleValue), r->ProcessId);
	if (hObject) {
		ObjectHelpers::ShowObjectProperties(hObject, r->Type, r->Name);
		::CloseHandle(hObject);
		return;
	}
	AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
}

void CSearchView::UpdateStatusText() const {
	if (!IsActive())
		return;
	if (m_Searching)
		GetFrame()->SetStatusText(7, std::format(L"Searching: {} / {} handles, {} found", (ULONG)m_Searched, (ULONG)m_Total, m_Results.size()).c_str());
	else
		GetFrame()->SetStatusText(7, std::format(L"Found: {}", m_Results.size()).c_str());
}

LRESULT CSearchView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Type", LVCFMT_LEFT, 140, ColumnType::Type);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 450, ColumnType::Name);
	cm->AddColumn(L"Process Name", LVCFMT_LEFT, 150, ColumnType::ProcessName);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 70, ColumnType::PID, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Handle", LVCFMT_RIGHT, 80, ColumnType::Handle, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Address", LVCFMT_RIGHT, 130, ColumnType::Address, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	StartSearch();
	return 0;
}

LRESULT CSearchView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	m_Cancel = true;
	while (m_WorkerRunning)
		::Sleep(50);
	handled = FALSE;
	return 0;
}

LRESULT CSearchView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List, L","));
	return 0;
}

LRESULT CSearchView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (row >= 0)
		ShowProperties(row);
	return 0;
}

LRESULT CSearchView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}
