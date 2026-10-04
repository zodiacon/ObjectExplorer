#include "pch.h"
#include "ProcessesView.h"
#include <TlHelp32.h>
#include "ProcessHelper.h"
#include "ObjectHelpers.h"
#include "DriverHelper.h"
#include "ImageIconCache.h"
#include "ResourceManager.h"
#include "SortHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "ViewFactory.h"

namespace {
	CString FormatTime(FILETIME const& ft) {
		if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0)
			return L"";
		return CTime(ft).Format(L"%x %X");
	}

	CString FormatCPUTime(FILETIME const& kernel, FILETIME const& user) {
		auto total = (*(ULONGLONG const*)&kernel + *(ULONGLONG const*)&user) / 10000;	// msec
		return std::format(L"{}.{:03}", (PCWSTR)CTimeSpan(total / 1000).Format(L"%H:%M:%S"), total % 1000).c_str();
	}

	ULONGLONG ToULong64(FILETIME const& ft) {
		return *(ULONGLONG const*)&ft;
	}
}

CProcessesView::CProcessesView(IMainFrame* frame, bool threads) : CViewBase(frame), m_Threads(threads) {
}

CString CProcessesView::GetTitle() const {
	return m_Threads ? L"Threads" : L"Processes";
}

void CProcessesView::Refresh() {
	CWaitCursor wait;
	m_Items.clear();

	wil::unique_handle hSnapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | (m_Threads ? TH32CS_SNAPTHREAD : 0), 0));
	if (hSnapshot) {
		std::unordered_map<DWORD, CString> names;
		PROCESSENTRY32 pe{ sizeof(pe) };
		if (::Process32First(hSnapshot.get(), &pe)) {
			do {
				names[pe.th32ProcessID] = pe.szExeFile;
				if (!m_Threads) {
					Item item;
					item.Id = item.ProcessId = pe.th32ProcessID;
					item.ParentId = pe.th32ParentProcessID;
					item.Threads = pe.cntThreads;
					item.Priority = pe.pcPriClassBase;
					item.Name = pe.szExeFile;
					m_Items.push_back(std::move(item));
				}
			} while (::Process32Next(hSnapshot.get(), &pe));
		}
		if (m_Threads) {
			THREADENTRY32 te{ sizeof(te) };
			if (::Thread32First(hSnapshot.get(), &te)) {
				do {
					Item item;
					item.Id = te.th32ThreadID;
					item.ProcessId = item.ParentId = te.th32OwnerProcessID;
					item.Threads = 0;
					item.Priority = te.tpBasePri;
					item.Name = names[te.th32OwnerProcessID];
					m_Items.push_back(std::move(item));
				} while (::Thread32Next(hSnapshot.get(), &te));
			}
		}
	}

	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	UpdateStatusText();
}

void CProcessesView::GetDetails(Item const& item) const {
	if (item.DetailsChecked)
		return;
	item.DetailsChecked = true;

	if (m_Threads) {
		wil::unique_handle hThread(::OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, item.Id));
		FILETIME exit;
		if (hThread)
			::GetThreadTimes(hThread.get(), &item.CreateTime, &exit, &item.KernelTime, &item.UserTime);
		return;
	}

	::ProcessIdToSessionId(item.Id, &item.Session);
	item.User = ProcessHelper::GetUserName(item.Id).c_str();
	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, item.Id));
	if (hProcess) {
		::GetProcessHandleCount(hProcess.get(), &item.Handles);
		FILETIME exit;
		::GetProcessTimes(hProcess.get(), &item.CreateTime, &exit, &item.KernelTime, &item.UserTime);
		WCHAR path[MAX_PATH];
		DWORD size = _countof(path);
		if (::QueryFullProcessImageName(hProcess.get(), 0, path, &size))
			item.ImagePath = path;
	}
}

void CProcessesView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	// these columns need the details of every item
	switch (col) {
		case ColumnType::Session: case ColumnType::Handles: case ColumnType::User:
		case ColumnType::Created: case ColumnType::CPUTime: case ColumnType::ImagePath:
		{
			CWaitCursor wait;
			for (auto& item : m_Items)
				GetDetails(item);
			break;
		}
	}
	std::sort(m_Items.begin(), m_Items.end(), [&](auto const& i1, auto const& i2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(i1.Name, i2.Name, asc);
			case ColumnType::Id: return SortHelper::Sort(i1.Id, i2.Id, asc);
			case ColumnType::ProcessId: return SortHelper::Sort(i1.ProcessId, i2.ProcessId, asc);
			case ColumnType::ParentId: return SortHelper::Sort(i1.ParentId, i2.ParentId, asc);
			case ColumnType::Session: return SortHelper::Sort(i1.Session, i2.Session, asc);
			case ColumnType::Threads: return SortHelper::Sort(i1.Threads, i2.Threads, asc);
			case ColumnType::Handles: return SortHelper::Sort(i1.Handles, i2.Handles, asc);
			case ColumnType::Priority: return SortHelper::Sort(i1.Priority, i2.Priority, asc);
			case ColumnType::User: return SortHelper::Sort(i1.User, i2.User, asc);
			case ColumnType::ImagePath: return SortHelper::Sort(i1.ImagePath, i2.ImagePath, asc);
			case ColumnType::Created: return SortHelper::Sort(ToULong64(i1.CreateTime), ToULong64(i2.CreateTime), asc);
			case ColumnType::CPUTime:
				return SortHelper::Sort(ToULong64(i1.KernelTime) + ToULong64(i1.UserTime), ToULong64(i2.KernelTime) + ToULong64(i2.UserTime), asc);
		}
		return false;
		});
}

CString CProcessesView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	auto column = GetColumnManager(m_List)->GetColumnTag<ColumnType>(col);
	switch (column) {
		case ColumnType::Name: return item.Name;
		case ColumnType::Id: return std::to_wstring(item.Id).c_str();
		case ColumnType::ProcessId: return std::to_wstring(item.ProcessId).c_str();
		case ColumnType::ParentId: return item.ParentId ? std::to_wstring(item.ParentId).c_str() : L"";
		case ColumnType::Threads: return std::to_wstring(item.Threads).c_str();
		case ColumnType::Priority: return std::to_wstring(item.Priority).c_str();
	}

	GetDetails(item);
	switch (column) {
		case ColumnType::Session: return item.Session == (DWORD)-1 ? L"" : std::to_wstring(item.Session).c_str();
		case ColumnType::Handles: return item.Handles ? std::to_wstring(item.Handles).c_str() : L"";
		case ColumnType::User: return item.User;
		case ColumnType::ImagePath: return item.ImagePath;
		case ColumnType::Created: return FormatTime(item.CreateTime);
		case ColumnType::CPUTime: return item.CreateTime.dwHighDateTime ? FormatCPUTime(item.KernelTime, item.UserTime) : CString();
	}
	return L"";
}

int CProcessesView::GetRowImage(HWND, int row, int col) const {
	if (m_Threads)
		return ResourceManager::Get().GetTypeImage(L"Thread");

	auto& item = m_Items[row];
	GetDetails(item);
	if (item.ImagePath.IsEmpty())
		return 0;
	return ImageIconCache::Get().GetIcon((PCWSTR)item.ImagePath);
}

void CProcessesView::UpdateUI(bool force) {
	auto& ui = UI();
	int selected = m_List.GetSelectedCount();
	ui.UIEnable(ID_VIEW_PROPERTIES, selected == 1);
	ui.UIEnable(ID_EDIT_COPY, selected > 0);
}

bool CProcessesView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row < 0)
		return false;
	if (m_Threads)
		ShowProperties(row);
	else
		ViewFactory::Get().CreateView(ViewType::ProcessHandles, m_Items[row].Id);
	return true;
}

void CProcessesView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CProcessesView::OnPageActivated(bool active) {
	if (active) {
		UpdateUI();
		UpdateStatusText();
	}
}

void CProcessesView::ShowProperties(int row) const {
	auto& item = m_Items[row];
	auto id = item.Id;
	auto name = item.Name;
	HANDLE hObject;
	if (m_Threads) {
		hObject = ::OpenThread(THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id);
		if (!hObject)
			hObject = DriverHelper::OpenThread(id, THREAD_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
	}
	else {
		hObject = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, id);
		if (!hObject)
			hObject = DriverHelper::OpenProcess(id, PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE);
	}
	if (!hObject) {
		AtlMessageBox(m_hWnd, L"Error opening object.", IDS_TITLE, MB_ICONERROR);
		return;
	}
	ObjectHelpers::ShowObjectProperties(hObject, m_Threads ? L"Thread" : L"Process", std::format(L"{} ({})", (PCWSTR)name, id).c_str());
	::CloseHandle(hObject);
}

void CProcessesView::UpdateStatusText() const {
	if (IsActive())
		GetFrame()->SetStatusText(7, std::format(L"{}: {}", (PCWSTR)GetTitle(), m_Items.size()).c_str());
}

LRESULT CProcessesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);
	auto numeric = ColumnFlags::Visible | ColumnFlags::Numeric;

	if (m_Threads) {
		cm->AddColumn(L"TID", LVCFMT_RIGHT, 80, ColumnType::Id, numeric);
		cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::ProcessId, numeric);
		cm->AddColumn(L"Process Name", LVCFMT_LEFT, 200, ColumnType::Name);
		cm->AddColumn(L"Priority", LVCFMT_RIGHT, 70, ColumnType::Priority, numeric);
		cm->AddColumn(L"Created", LVCFMT_LEFT, 160, ColumnType::Created);
		cm->AddColumn(L"CPU Time", LVCFMT_RIGHT, 110, ColumnType::CPUTime, numeric);
	}
	else {
		cm->AddColumn(L"Name", LVCFMT_LEFT, 200, ColumnType::Name);
		cm->AddColumn(L"PID", LVCFMT_RIGHT, 80, ColumnType::Id, numeric);
		cm->AddColumn(L"Parent PID", LVCFMT_RIGHT, 80, ColumnType::ParentId, numeric);
		cm->AddColumn(L"Session", LVCFMT_RIGHT, 60, ColumnType::Session, numeric);
		cm->AddColumn(L"Threads", LVCFMT_RIGHT, 70, ColumnType::Threads, numeric);
		cm->AddColumn(L"Handles", LVCFMT_RIGHT, 70, ColumnType::Handles, numeric);
		cm->AddColumn(L"Priority", LVCFMT_RIGHT, 60, ColumnType::Priority, numeric);
		cm->AddColumn(L"User", LVCFMT_LEFT, 200, ColumnType::User);
		cm->AddColumn(L"Created", LVCFMT_LEFT, 160, ColumnType::Created);
		cm->AddColumn(L"CPU Time", LVCFMT_RIGHT, 110, ColumnType::CPUTime, numeric);
		cm->AddColumn(L"Image Path", LVCFMT_LEFT, 350, ColumnType::ImagePath);
	}
	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(m_Threads ? ResourceManager::Get().GetTypesImageList() : ImageIconCache::Get().GetImageList(), LVSIL_SMALL);

	Refresh();
	return 0;
}

LRESULT CProcessesView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List, L","));
	return 0;
}

LRESULT CProcessesView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (row >= 0)
		ShowProperties(row);
	return 0;
}

LRESULT CProcessesView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}
