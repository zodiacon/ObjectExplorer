#include "pch.h"
#include "ZombieProcessesView.h"
#include "ObjectManager.h"
#include "ProcessHelper.h"
#include "StringHelper.h"
#include <SortHelper.h>
#include "ProcessHelper.h"
#include "ImageIconCache.h"

CString CZombieProcessesView::GetTitle() const {
	return L"Zombie Processes";
}

void CZombieProcessesView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto compare = [&](auto const& p1, auto const& p2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(p1.Name, p2.Name, si->SortAscending);
			case ColumnType::Pid: return SortHelper::Sort(p1.Pid, p2.Pid, si->SortAscending);
			case ColumnType::UserTime: return SortHelper::Sort(p1.UserTime, p2.UserTime, si->SortAscending);
			case ColumnType::KernelTime: return SortHelper::Sort(p1.KernelTime, p2.KernelTime, si->SortAscending);
			case ColumnType::CPUTime: return SortHelper::Sort(p1.KernelTime + p1.UserTime, p2.KernelTime + p2.UserTime, si->SortAscending);
			case ColumnType::CreateTime: return SortHelper::Sort(p1.CreateTime, p2.CreateTime, si->SortAscending);
			case ColumnType::ExitTime: return SortHelper::Sort(p1.ExitTime, p2.ExitTime, si->SortAscending);
			case ColumnType::ExitCode: return SortHelper::Sort(p1.ExitCode, p2.ExitCode, si->SortAscending);
			case ColumnType::Handles: return SortHelper::Sort(p1.Handles.size(), p2.Handles.size(), si->SortAscending);
		}
		return false;
	};
	std::ranges::sort(m_Items, compare);
}

CString CZombieProcessesView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Name: return item.Name.c_str();
		case ColumnType::Pid: return std::format(L"{}", item.Pid).c_str();
		case ColumnType::Handles: return std::format("{}", item.Handles.size()).c_str();
		case ColumnType::ExitCode: return std::format("0x{:X}", item.ExitCode).c_str();
		case ColumnType::CreateTime: return CTime(*(FILETIME*)&item.CreateTime).Format(L"%x %X");
		case ColumnType::ExitTime: return CTime(*(FILETIME*)&item.ExitTime).Format(L"%x %X");
		case ColumnType::KernelTime: return StringHelper::TimeSpanToString(item.KernelTime);
		case ColumnType::UserTime: return StringHelper::TimeSpanToString(item.UserTime);
		case ColumnType::CPUTime: return StringHelper::TimeSpanToString(item.UserTime + item.KernelTime);
		case ColumnType::Details:
			CString text;
			// list the first 5 handles
			auto count = std::min((int)item.Handles.size(), 5);
			for (auto i = 0; i < count; i++) {
				auto& hi = item.Handles[i];
				text += std::format(L"H: 0x{:X} PID: {} ({}) ", hi.Handle, hi.Pid, ProcessHelper::GetProcessName2(hi.Pid)).c_str();
			}
			return text;
	}
	return CString();
}

int CZombieProcessesView::GetRowImage(HWND, int row, int col) const {
	return ImageIconCache::Get().GetIcon(m_Items[row].FullPath);
}

int CZombieProcessesView::GetSaveColumnRange(int& start) const {
	return 2;
}

bool CZombieProcessesView::IsSortable(HWND, int col) const {
	return GetColumnManager(m_List)->GetColumnTag<ColumnType>(col) != ColumnType::Details;
}

void CZombieProcessesView::Refresh() {
	CWaitCursor wait;
	m_Items.clear();
	m_Items.reserve(128);
	std::unordered_map<DWORD, size_t> processes;
	for (auto const& h : ObjectManager::EnumHandles2(L"Process")) {
		auto hDup = ObjectManager::DupHandle((HANDLE)(ULONG_PTR)h->HandleValue , h->ProcessId, SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION);
		if (hDup && WAIT_OBJECT_0 == ::WaitForSingleObject(hDup, 0)) {
			//
			// zombie process
			//
			auto pid = ::GetProcessId(hDup);
			if (pid) {
				auto it = processes.find(pid);
				ZombieProcess zp;
				auto& z = it == processes.end() ? zp : m_Items[it->second];
				z.Pid = pid;
				z.Handles.push_back({ h->HandleValue, h->ProcessId });
				WCHAR name[MAX_PATH];
				if (::GetProcessImageFileName(hDup, name, _countof(name))) {
					z.FullPath = ProcessHelper::GetDosNameFromNtName(name);
					z.Name = wcsrchr(name, L'\\') + 1;
				}
				::GetProcessTimes(hDup, (PFILETIME)&z.CreateTime, (PFILETIME)&z.ExitTime, (PFILETIME)&z.KernelTime, (PFILETIME)&z.UserTime);
				::GetExitCodeProcess(hDup, &z.ExitCode);
				if (it == processes.end()) {
					m_Items.push_back(std::move(z));
					processes.insert({ pid, m_Items.size() - 1 });
				}
			}
		}
		if (hDup)
			::CloseHandle(hDup);
	}
	Sort(m_List);
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
	GetFrame()->SetStatusText(7, std::format(L"Zombie Processes: {}", m_Items.size()).c_str());
}

LRESULT CZombieProcessesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(*this, rcDefault, nullptr, ListViewDefaultStyle | LVS_SHAREIMAGELISTS);
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT);
	m_List.SetImageList(ImageIconCache::Get().GetImageList(), LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Name", LVCFMT_LEFT, 230, ColumnType::Name, ColumnFlags::Visible);
	cm->AddColumn(L"PID", LVCFMT_RIGHT, 90, ColumnType::Pid, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Handles", LVCFMT_RIGHT, 80, ColumnType::Handles, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Exit Code", LVCFMT_RIGHT, 80, ColumnType::ExitCode, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Start Time", LVCFMT_RIGHT, 140, ColumnType::CreateTime, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Exit Time", LVCFMT_RIGHT, 140, ColumnType::ExitTime, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"CPU Time", LVCFMT_RIGHT, 110, ColumnType::CPUTime, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"User Time", LVCFMT_RIGHT, 110, ColumnType::UserTime, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Kernel Time", LVCFMT_RIGHT, 110, ColumnType::KernelTime, ColumnFlags::Visible | ColumnFlags::Numeric);
	cm->AddColumn(L"Details", LVCFMT_LEFT, 500, ColumnType::Details);
	cm->UpdateColumns();

	Refresh();

	return 0;
}

LRESULT CZombieProcessesView::OnRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}
