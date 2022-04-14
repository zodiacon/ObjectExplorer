#include "pch.h"
#include "HandlesView.h"
#include "ResourceManager.h"
#include "SortHelper.h"
#include "ProcessHelper.h"
#include "ObjectHelpers.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "StringHelper.h"
#include "AccessMaskDecoder.h"

CHandlesView::CHandlesView(IMainFrame* frame, DWORD pid)
	: CViewBase(frame), m_Tracker(m_Pid = pid) {
	if (pid)
		m_hProcess.reset(::OpenProcess(SYNCHRONIZE, FALSE, pid));
}

void CHandlesView::OnFinalMessage(HWND) {
	delete this;
}

CString CHandlesView::GetTitle() const {
	if (m_Pid == 0)
		return L"All Handles";

	CString title;
	title.Format(L"%s (PID: %u)", (PCWSTR)ProcessHelper::GetProcessName(m_Pid), m_Pid);
	return title;
}

void CHandlesView::Refresh() {
	CWaitCursor wait;
	m_Tracker.EnumHandles(true);
	m_Handles = m_Tracker.GetNewHandles();
	m_UpdateProcNames = m_UpdateObjectNames = false;
	for (auto& hi : m_Handles) {
		hi->Type = ObjectManager::GetType(hi->ObjectTypeIndex)->TypeName;
	}
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Handles.size(), LVSICF_NOSCROLL);
	UpdateStatusText();
}

void CHandlesView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = static_cast<ColumnType>(GetColumnManager(si->hWnd)->GetColumnTag(si->SortColumn));
	if (col == ColumnType::ProcessName && !m_UpdateProcNames) {
		CWaitCursor wait;
		for (auto& hi : m_Handles) {
			if (hi->ProcessName.IsEmpty())
				hi->ProcessName = ProcessHelper::GetProcessName(hi->ProcessId);
		}
		m_UpdateProcNames = true;
	}
	else if (col == ColumnType::Name && !m_UpdateObjectNames) {
		CWaitCursor wait;
		for (auto& hi : m_Handles) {
			if (!hi->NameChecked) {
				if(ObjectHelpers::IsNamedObjectType(hi->ObjectTypeIndex))
					hi->Name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId, hi->ObjectTypeIndex);
				hi->NameChecked = true;
			}
		}
		m_UpdateObjectNames = true;
	}
	auto asc = si->SortAscending;
	auto compare = [&](auto const& h1, auto const& h2) {
		switch (col) {
			case ColumnType::Address: return SortHelper::Sort(h1->Object, h2->Object, asc);
			case ColumnType::ProcessName: return SortHelper::Sort(h1->ProcessName, h2->ProcessName, asc);
			case ColumnType::Name: return SortHelper::Sort(h1->Name, h2->Name, asc);
			case ColumnType::Type: return SortHelper::Sort(h1->Type, h2->Type, asc);
			case ColumnType::Handle: return SortHelper::Sort(h1->HandleValue, h2->HandleValue, asc);
			case ColumnType::Attributes: return SortHelper::Sort(h1->HandleAttributes, h2->HandleAttributes, asc);
			case ColumnType::PID: return SortHelper::Sort(h1->ProcessId, h2->ProcessId, asc);
			case ColumnType::Access: 
			case ColumnType::DecodedAccess:
				return SortHelper::Sort(h1->GrantedAccess, h2->GrantedAccess, asc);
		}
		return false;
	};

	m_Handles.Sort(compare);
}

CString CHandlesView::GetColumnText(HWND h, int row, int col) const {
	auto& hi = m_Handles[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Type: return hi->Type;
		case ColumnType::Handle: return std::format("0x{:X}", hi->HandleValue).c_str();
		case ColumnType::Address: return std::format("0x{:X}", (ULONGLONG)hi->Object).c_str();
		case ColumnType::ProcessName:
			if (hi->ProcessName.IsEmpty())
				hi->ProcessName = ProcessHelper::GetProcessName(hi->ProcessId);
			return hi->ProcessName;
		case ColumnType::Access: return std::format("0x{:08X}", hi->GrantedAccess).c_str();
		case ColumnType::Name: 
			if (!hi->NameChecked) {
				hi->Name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId, hi->ObjectTypeIndex);
				hi->NameChecked = true;
			}
			return hi->Name.c_str();
		case ColumnType::PID: return std::to_wstring(hi->ProcessId).c_str();
		case ColumnType::Attributes: return StringHelper::HandleAttributesToString(hi->HandleAttributes);
		case ColumnType::DecodedAccess: return AccessMaskDecoder::DecodeAccessMask(hi->Type, hi->GrantedAccess);

	}
	return CString();
}

int CHandlesView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(m_Handles[row]->ObjectTypeIndex);
}

void CHandlesView::UpdateUI(bool force) {
	auto& ui = UI();
	int selected = m_List.GetSelectedCount();
	ui.UIEnable(ID_VIEW_PROPERTIES, selected == 1);
	ui.UIEnable(ID_EDIT_COPY, selected > 0);
}

bool CHandlesView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row >= 0)
		ShowObjectProperties(row);

	return true;
}

bool CHandlesView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	if (row >= 0) {
		CMenu menu;
		menu.LoadMenu(IDR_CONTEXT);
		auto running = IsRunning();
		if (running)
			Run(false, false);
		GetFrame()->TrackPopupMenu(menu.GetSubMenu(4), 0, pt.x, pt.y);
		if (running)
			Run(true, false);
		return true;
	}
	return false;
}

void CHandlesView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CHandlesView::OnPageActivated(bool active) {
	if (active) {
		UpdateUI();
		UI().UIEnable(ID_RUN, true);
		UpdateStatusText();
	}
	ActivateTimer(active);
}

void CHandlesView::ShowObjectProperties(int row) const {
	ATLASSERT(row >= 0);
	auto& hi = m_Handles[row];
	auto hObject = ObjectManager::DupHandle((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId);
	if (hObject) {
		ObjectHelpers::ShowObjectProperties(hObject, hi->Type, hi->Name.c_str());
		::CloseHandle(hObject);
		return;
	}
	AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
}

void CHandlesView::UpdateStatusText() const {
	GetFrame()->SetStatusText(7, std::format(L"Handles: {}", m_Handles.size()).c_str());
}

void CHandlesView::DoTimerWorkAsync() {
	auto tick = ::GetTickCount64();
	int count = (int)m_NewHandles.size();
	for (int i = 0; i < count; i++) {
		auto& hi = m_NewHandles[i];
		if (hi->TargetTime < tick) {
			hi->NewHandle = false;
			m_NewHandles.erase(m_NewHandles.begin() + i);
			i--;
			count--;
		}
	}

	m_Tracker.EnumHandles();
	m_TempHandles.clear();
	for (auto& hi : m_Tracker.GetNewHandles()) {
		hi->Type = ObjectManager::GetType(hi->ObjectTypeIndex)->TypeName;
		if (ObjectHelpers::IsNamedObjectType(hi->ObjectTypeIndex)) {
			hi->Name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId, hi->ObjectTypeIndex);
			hi->NameChecked = true;
		}
		if (m_Pid == 0)
			hi->ProcessName = ProcessHelper::GetProcessName(hi->ProcessId);
		hi->TargetTime = tick + 2000;
		hi->NewHandle = true;
		m_TempHandles.push_back(hi);
		m_NewHandles.push_back(hi);
	}
	for (auto& hi : m_Tracker.GetClosedHandles()) {
		hi->TargetTime = tick + 2000;
		hi->ClosedHandle = true;
	}

	PostMessage(WM_CONTINUEUPDATE);
	m_UpdateInProgress = false;
}

void CHandlesView::DoTimerUpdate() {
	if (m_hProcess && ::WaitForSingleObject(m_hProcess.get(), 0) == WAIT_OBJECT_0) {
		//
		// process dead
		//
		Run(false);
		UI().UIEnable(ID_RUN, false);
		m_Handles.clear();
		return;
	}
	Run(false, false);
	int start = std::max(0, m_List.GetTopIndex() - 10);
	int count = std::min(start + m_List.GetCountPerPage() + 100, (int)m_Handles.size());
	int orgCount = count;
	auto tick = ::GetTickCount64();
	for (int i = start; i < count; i++) {
		auto& hi = m_Handles[i];
		if (hi->ClosedHandle && hi->TargetTime < tick) {
			hi->ClosedHandle = false;
			m_Handles.Remove(i);		
			i--;
			count--;
		}
	}
	if (orgCount != count) {
		m_List.SetItemCountEx(count, LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
		m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	}
	m_UpdateInProgress = true;
	ATLVERIFY(::TrySubmitThreadpoolCallback([](auto, auto param) {
		return ((CHandlesView*)param)->DoTimerWorkAsync();
		}, this, nullptr));

}

DWORD CHandlesView::OnPrePaint(int, LPNMCUSTOMDRAW) {
	return CDRF_NOTIFYITEMDRAW;
}

DWORD CHandlesView::OnItemPrePaint(int, LPNMCUSTOMDRAW cd) {
	ATLASSERT(m_List == cd->hdr.hwndFrom);

	auto row = (int)cd->dwItemSpec;
	auto lv = (NMLVCUSTOMDRAW*)cd;

	auto& hi = m_Handles[row];
	if (hi->NewHandle) {
		lv->clrTextBk = RGB(0, 220, 64);
	}
	else if (hi->ClosedHandle)
		lv->clrTextBk = RGB(220, 80, 32);
	return CDRF_SKIPPOSTPAINT;
}

LRESULT CHandlesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Type", LVCFMT_LEFT, 160, ColumnType::Type);
	cm->AddColumn(L"Handle", LVCFMT_RIGHT, 90, ColumnType::Handle, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 300, ColumnType::Name);
	cm->AddColumn(L"Address", LVCFMT_RIGHT, 130, ColumnType::Address, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Access", LVCFMT_RIGHT, 100, ColumnType::Access, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Attributes", LVCFMT_RIGHT, 120, ColumnType::Attributes);
	if (m_Pid == 0) {
		cm->AddColumn(L"Process Name", LVCFMT_LEFT, 150, ColumnType::ProcessName);
		cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::PID);
	}
	cm->AddColumn(L"Decoded Access", LVCFMT_LEFT, 350, ColumnType::DecodedAccess);

	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	m_NewHandles.reserve(m_Pid ? 32 : 1024);

	Refresh();
	Run(true);

	return 0;
}

LRESULT CHandlesView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	auto text = ListViewHelper::GetSelectedRowsAsString(m_List, L",");
	ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CHandlesView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	ShowObjectProperties(row);
	return 0;
}

LRESULT CHandlesView::OnPauseResume(WORD, WORD, HWND, BOOL&) {
	Run(!IsRunning());
	return 0;
}

LRESULT CHandlesView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

LRESULT CHandlesView::OnContinueUpdate(UINT, WPARAM, LPARAM, BOOL&) {
	for (auto& hi : m_TempHandles)
		m_Handles.push_back(hi);
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Handles.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	if (IsRunning()) {
		Run(true, false);
		UpdateStatusText();
	}

	return 0;
}

LRESULT CHandlesView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	Run(false, false);
	while (m_UpdateInProgress) {
		::Sleep(100);
	}
	handled = FALSE;
	return 0;
}

