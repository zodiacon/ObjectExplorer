#include "pch.h"
#include "ObjectsView.h"
#include "ResourceManager.h"
#include "ProcessHelper.h"
#include "ObjectHelpers.h"
#include "SortHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"

CObjectsView::CObjectsView(IMainFrame* frame, PCWSTR type) : CViewBase(frame), m_TypeName(type), m_Tracker(type) {
}

CString CObjectsView::GetTitle() const {
	if (m_TypeName.IsEmpty())
		return L"All Objects";

	return L"Objects (" + m_TypeName + L")";
}

void CObjectsView::Refresh() {
	CWaitCursor wait;
	m_Tracker.EnumObjects(true);
	m_Objects = m_Tracker.GetNewObjects();
	m_UpdateProcNames = m_UpdateObjectNames = false;
	for (auto& obj : m_Objects) {
		obj->Type = ObjectManager::GetType(obj->TypeIndex)->TypeName;
	}
	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Objects.size(), LVSICF_NOSCROLL);
	UpdateStatusText();
}

void CObjectsView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = static_cast<ColumnType>(GetColumnManager(si->hWnd)->GetColumnTag(si->SortColumn));
	if (col == ColumnType::Name && !m_UpdateObjectNames) {
		CWaitCursor wait;
		for (auto& obj : m_Objects) {
			if (!obj->NameChecked) {
				if (ObjectHelpers::IsNamedObjectType(obj->TypeIndex)) {
					auto& hi = obj->FirstHandle;
					obj->Name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi.HandleValue, hi.ProcessId, obj->TypeIndex);
				}
				obj->NameChecked = true;
			}
		}
		m_UpdateObjectNames = true;
	}
	auto asc = si->SortAscending;
	auto compare = [&](auto const& obj1, auto const& obj2) {
		switch (col) {
			case ColumnType::Address: return SortHelper::Sort(obj1->Object, obj2->Object, asc);
			case ColumnType::Name: return SortHelper::Sort(obj1->Name, obj2->Name, asc);
			case ColumnType::Type: return SortHelper::Sort(obj1->Type, obj2->Type, asc);
			case ColumnType::Handles: return SortHelper::Sort(obj1->ManualHandleCount, obj2->ManualHandleCount, asc);
			case ColumnType::RefCount: return SortHelper::Sort(obj1->PointerCount, obj2->PointerCount, asc);
		}
		return false;
	};

	m_Objects.Sort(compare);
}

int CObjectsView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(m_Objects[row]->TypeIndex);
}

int CObjectsView::GetSaveColumnRange(HWND, int& start) const {
	start = 2;
	return 1;
}

void CObjectsView::UpdateStatusText() const {
	if(IsActive())
		GetFrame()->SetStatusText(7, std::format(L"Objects: {}", m_Objects.size()).c_str());
}

CString CObjectsView::GetColumnText(HWND h, int row, int col) const {
	auto& obj = m_Objects[row];
	switch (GetColumnManager(h)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Type: return obj->Type;
		case ColumnType::Handles: return std::format("{}", obj->HandleCount == 0 ? obj->ManualHandleCount : obj->HandleCount).c_str();
		case ColumnType::RefCount: return obj->PointerCount == 0 ? L"<access denied>" : std::format(L"0x{:X} ({})", obj->PointerCount, obj->PointerCount).c_str();
		case ColumnType::Address: return std::format(L"0x{:X}", (ULONGLONG)obj->Object).c_str();
		case ColumnType::Name:
			if (!obj->NameChecked) {
				auto& hi = obj->FirstHandle;
				obj->Name = ObjectManager::GetObjectName((HANDLE)(ULONG_PTR)hi.HandleValue, hi.ProcessId, obj->TypeIndex);
				obj->NameChecked = true;
			}
			return obj->Name.c_str();
	}
	ATLASSERT(false);
	return CString();
}

LRESULT CObjectsView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Type", LVCFMT_LEFT, 160, ColumnType::Type);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 450, ColumnType::Name);
	cm->AddColumn(L"Address", LVCFMT_RIGHT, 130, ColumnType::Address, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Handles", LVCFMT_RIGHT, 90, ColumnType::Handles, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"References", LVCFMT_RIGHT, 140, ColumnType::RefCount, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	Refresh();

	return 0;
}

LRESULT CObjectsView::OnDestroy(UINT, WPARAM, LPARAM, BOOL& handled) {
	while (m_UpdateInProgress) {
		::Sleep(100);
	}
	handled = FALSE;
	return 0;
}

LRESULT CObjectsView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	auto text = ListViewHelper::GetSelectedRowsAsString(m_List, L",");
	ClipboardHelper::CopyText(m_hWnd, text);
	return 0;
}

LRESULT CObjectsView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

LRESULT CObjectsView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	ATLASSERT(m_List.GetSelectedCount() == 1);
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	ShowObjectProperties(row);
	return 0;
}

bool CObjectsView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row >= 0)
		ShowObjectProperties(row);

	return true;
}

void CObjectsView::OnPageActivated(bool active) {
	if (active) {
		UpdateStatusText();
	}
}

void CObjectsView::ShowObjectProperties(int row) const {
	ATLASSERT(row >= 0);
	auto& obj = m_Objects[row];
	auto& hi = obj->FirstHandle;
	auto hObject = ObjectManager::DupHandle((HANDLE)(ULONG_PTR)hi.HandleValue, hi.ProcessId);
	if (hObject) {
		ObjectHelpers::ShowObjectProperties(hObject, obj->Type, obj->Name.c_str());
		::CloseHandle(hObject);
		return;
	}
	AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
}
