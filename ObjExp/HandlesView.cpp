#include "pch.h"
#include "HandlesView.h"
#include "ResourceManager.h"
#include "SortHelper.h"
#include "ProcessHelper.h"
#include "ObjectHelpers.h"

CHandlesView::CHandlesView(IMainFrame* frame, DWORD pid, PCWSTR name)
	: CViewBase(frame), m_Tracker(m_Pid = pid), m_ProcessName(name) {
}

void CHandlesView::OnFinalMessage(HWND) {
	delete this;
}

CString CHandlesView::GetTitle() const {
	if (m_Pid == 0)
		return L"All Handles";

	CString title;
	title.Format(L"%s (PID: %u)", (PCWSTR)m_ProcessName, m_Pid);
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
			case ColumnType::Access: return SortHelper::Sort(h1->GrantedAccess, h2->GrantedAccess, asc);
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
	}
	return CString();
}

int CHandlesView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(m_Handles[row]->ObjectTypeIndex);
}

LRESULT CHandlesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(*this, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Type", LVCFMT_LEFT, 120, ColumnType::Type);
	cm->AddColumn(L"Handle", LVCFMT_RIGHT, 100, ColumnType::Handle, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 300, ColumnType::Name);
	cm->AddColumn(L"Address", LVCFMT_RIGHT, 130, ColumnType::Address, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Access", LVCFMT_RIGHT, 100, ColumnType::Access, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Attributes", LVCFMT_RIGHT, 100, ColumnType::Attributes);
	if (m_Pid == 0) {
		cm->AddColumn(L"Process Name", LVCFMT_LEFT, 150, ColumnType::ProcessName);
		cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::PID);
	}
	cm->AddColumn(L"Decoded Access", LVCFMT_LEFT, 230, ColumnType::DecodedAccess);

	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);
	Refresh();

	return 0;
}

LRESULT CHandlesView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	return LRESULT();
}

LRESULT CHandlesView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	auto& hi = m_Handles[m_List.GetNextItem(-1, LVNI_SELECTED)];
	auto hObject = ObjectManager::DupHandle((HANDLE)(ULONG_PTR)hi->HandleValue, hi->ProcessId);
	if (hObject) {
		ObjectHelpers::ShowObjectProperties(hObject, hi->Type, hi->Name.c_str());
		::CloseHandle(hObject);
		return 0;
	}
	AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
	return 0;
}

LRESULT CHandlesView::OnPauseResume(WORD, WORD, HWND, BOOL&) {
	return LRESULT();
}
