// View.cpp : implementation of the CObjectTypesView class
//
/////////////////////////////////////////////////////////////////////////////

#include "pch.h"
#include "resource.h"
#include "ObjectTypesView.h"
#include "ResourceManager.h"
#include "StringHelper.h"
#include "SortHelper.h"

BOOL CObjectTypesView::PreTranslateMessage(MSG* pMsg) {
	pMsg;
	return FALSE;
}

void CObjectTypesView::OnFinalMessage(HWND /*hWnd*/) {
	delete this;
}

CString CObjectTypesView::GetTitle() const {
	return L"Object Types";
}

int CObjectTypesView::GetIconIndex() const {
	return 0;
}

CString CObjectTypesView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	CString text;
	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Name: return item->TypeName;
		case ColumnType::Index: return std::to_wstring(item->TypeIndex).c_str();
		case ColumnType::Objects: return std::to_wstring(item->TotalNumberOfObjects).c_str();
		case ColumnType::Handles: return std::to_wstring(item->TotalNumberOfHandles).c_str();
		case ColumnType::PeakObjects: return std::to_wstring(item->HighWaterNumberOfObjects).c_str();
		case ColumnType::PeakHandles: return std::to_wstring(item->HighWaterNumberOfHandles).c_str();
		case ColumnType::DefaultPaged: return std::to_wstring(item->DefaultPagedPoolCharge).c_str();
		case ColumnType::DefaultNonPaged: return std::to_wstring(item->DefaultNonPagedPoolCharge).c_str();
		case ColumnType::ValidAccess: text.Format(L"0x%08X", item->ValidAccessMask); break;
		case ColumnType::Pool: return StringHelper::PoolTypeToString(item->PoolType);
		case ColumnType::GenericRead:
			text.Format(L"0x%08X", item->GenericMapping.GenericRead);
			break;
		case ColumnType::GenericWrite:
			text.Format(L"0x%08X", item->GenericMapping.GenericWrite);
			break;
		case ColumnType::GenericExecute:
			text.Format(L"0x%08X", item->GenericMapping.GenericExecute);
			break;
		case ColumnType::GenericAll:
			text.Format(L"0x%08X", item->GenericMapping.GenericAll);
			break;
		case ColumnType::InvalidAttributes:
			text.Format(L"0x%04X", item->InvalidAttributes);
			break;
	}
	return text;
}

int CObjectTypesView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(m_Items[row]->TypeIndex);
}

LRESULT CObjectTypesView::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	m_hWndClient = m_List.Create(*this, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Name", LVCFMT_LEFT, 180, ColumnType::Name, ColumnFlags::Visible);
	cm->AddColumn(L"Index", LVCFMT_RIGHT, 50, ColumnType::Index, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Objects", LVCFMT_RIGHT, 100, ColumnType::Objects, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Handles", LVCFMT_RIGHT, 100, ColumnType::Handles, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Peak Objects", LVCFMT_RIGHT, 100, ColumnType::PeakObjects, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Peak Handles", LVCFMT_RIGHT, 100, ColumnType::PeakHandles, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Pool Type", LVCFMT_LEFT, 110, ColumnType::Pool);
	cm->AddColumn(L"Default Paged Charge", LVCFMT_RIGHT, 130, ColumnType::DefaultPaged, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Default NP Charge", LVCFMT_RIGHT, 130, ColumnType::DefaultNonPaged, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Valid Access Mask", LVCFMT_RIGHT, 110, ColumnType::ValidAccess, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Generic Read", LVCFMT_RIGHT, 110, ColumnType::GenericRead);
	cm->AddColumn(L"Generic Write", LVCFMT_RIGHT, 110, ColumnType::GenericWrite);
	cm->AddColumn(L"Generic Execute", LVCFMT_RIGHT, 110, ColumnType::GenericExecute);
	cm->AddColumn(L"Generic All", LVCFMT_RIGHT, 110, ColumnType::GenericAll);
	cm->AddColumn(L"Invalid Attr", LVCFMT_RIGHT, 90, ColumnType::InvalidAttributes);

	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	auto count = m_mgr.EnumTypes();
	m_Items = m_mgr.GetObjectTypes();
	m_List.SetItemCount(count);

	return 0;
}

void CObjectTypesView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;
	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;

	auto compare = [&](auto& item1, auto& item2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(item1->TypeName, item2->TypeName, asc);
			case ColumnType::Index: return SortHelper::Sort(item1->TypeIndex, item2->TypeIndex, asc);
			case ColumnType::Objects: return SortHelper::Sort(item1->TotalNumberOfObjects, item2->TotalNumberOfObjects, asc);
			case ColumnType::Handles: return SortHelper::Sort(item1->TotalNumberOfHandles, item2->TotalNumberOfHandles, asc);
			case ColumnType::PeakHandles: return SortHelper::Sort(item1->HighWaterNumberOfObjects, item2->HighWaterNumberOfObjects, asc);
			case ColumnType::PeakObjects: return SortHelper::Sort(item1->HighWaterNumberOfHandles, item2->HighWaterNumberOfHandles, asc);
			case ColumnType::Pool: return SortHelper::Sort(item1->PoolType, item2->PoolType, asc);
			case ColumnType::DefaultNonPaged: return SortHelper::Sort(item1->DefaultNonPagedPoolCharge, item2->DefaultNonPagedPoolCharge, asc);
			case ColumnType::DefaultPaged: return SortHelper::Sort(item1->DefaultPagedPoolCharge, item2->DefaultPagedPoolCharge, asc);
			case ColumnType::ValidAccess: return SortHelper::Sort(item1->ValidAccessMask, item2->ValidAccessMask, asc);
			case ColumnType::GenericRead: return SortHelper::Sort(item1->GenericMapping.GenericRead, item2->GenericMapping.GenericRead, asc);
			case ColumnType::GenericWrite: return SortHelper::Sort(item1->GenericMapping.GenericWrite, item2->GenericMapping.GenericWrite, asc);
			case ColumnType::GenericExecute: return SortHelper::Sort(item1->GenericMapping.GenericExecute, item2->GenericMapping.GenericExecute, asc);
			case ColumnType::GenericAll: return SortHelper::Sort(item1->GenericMapping.GenericAll, item2->GenericMapping.GenericAll, asc);
		}
		return false;
	};
	std::sort(m_Items.begin(), m_Items.end(), compare);
}
