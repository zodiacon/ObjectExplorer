#include "pch.h"
#include "SystemInfoView.h"
#include "ObjectManager.h"
#include "DriverHelper.h"
#include "DbgDriver.h"
#include "SecurityHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"

extern "C" LONG NTAPI RtlGetVersion(PRTL_OSVERSIONINFOW versionInfo);

namespace {
	CString FormatMemory(ULONGLONG bytes) {
		return std::format(L"{:L} MB", bytes >> 20).c_str();
	}

	CString FormatCount(ULONGLONG count) {
		return std::format(L"{:L}", count).c_str();
	}
}

CString CSystemInfoView::GetTitle() const {
	return L"System Information";
}

void CSystemInfoView::Refresh() {
	std::vector<std::pair<CString, CString>> items;
	auto add = [&](PCWSTR name, CString const& value) {
		items.push_back({ name, value });
	};

	WCHAR computer[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD size = _countof(computer);
	if (::GetComputerName(computer, &size))
		add(L"Computer Name", computer);

	RTL_OSVERSIONINFOW version{ sizeof(version) };
	if (RtlGetVersion(&version) == 0)
		add(L"Windows Version", std::format(L"{}.{}.{}", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber).c_str());

	add(L"Processors", std::to_wstring(::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)).c_str());

	auto uptime = ::GetTickCount64() / 1000;
	add(L"Up Time", std::format(L"{} days, {:02}:{:02}:{:02}", uptime / 86400, uptime / 3600 % 24, uptime / 60 % 60, uptime % 60).c_str());

	MEMORYSTATUSEX ms{ sizeof(ms) };
	if (::GlobalMemoryStatusEx(&ms)) {
		add(L"Physical Memory", FormatMemory(ms.ullTotalPhys));
		add(L"Available Memory", FormatMemory(ms.ullAvailPhys));
	}

	PERFORMANCE_INFORMATION pi{ sizeof(pi) };
	if (::GetPerformanceInfo(&pi, sizeof(pi))) {
		auto page = (ULONGLONG)pi.PageSize;
		add(L"Commit Charge", FormatMemory(pi.CommitTotal * page));
		add(L"Commit Limit", FormatMemory(pi.CommitLimit * page));
		add(L"Commit Peak", FormatMemory(pi.CommitPeak * page));
		add(L"Kernel Paged Pool", FormatMemory(pi.KernelPaged * page));
		add(L"Kernel Non-Paged Pool", FormatMemory(pi.KernelNonpaged * page));
		add(L"System Cache", FormatMemory(pi.SystemCache * page));
		add(L"Processes", FormatCount(pi.ProcessCount));
		add(L"Threads", FormatCount(pi.ThreadCount));
		add(L"Handles", FormatCount(pi.HandleCount));
	}

	ObjectManager::EnumTypes();
	add(L"Object Types", FormatCount(ObjectManager::GetObjectTypes().size()));
	add(L"Objects (Peak)", std::format(L"{:L} ({:L})", ObjectManager::TotalObjects, ObjectManager::PeakObjects).c_str());
	add(L"Handles (Peak)", std::format(L"{:L} ({:L})", ObjectManager::TotalHandles, ObjectManager::PeakHandles).c_str());

	add(L"Running Elevated", SecurityHelper::IsRunningElevated() ? L"Yes" : L"No");
	add(L"Object Explorer Driver", DriverHelper::IsDriverLoaded() ? L"Loaded" : L"Not loaded");
	add(L"Kernel Debugger Driver", DbgDriver::Get() ? L"Open" : L"Not open");

	bool sameRows = items.size() == m_Items.size();
	m_Items = std::move(items);
	if (sameRows)
		m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	else
		m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
}

CString CSystemInfoView::GetColumnText(HWND, int row, int col) const {
	auto& [name, value] = m_Items[row];
	return col == 0 ? name : value;
}

void CSystemInfoView::UpdateUI(bool force) {
	UI().UIEnable(ID_EDIT_COPY, m_List.GetSelectedCount() > 0);
	UI().UIEnable(ID_VIEW_PROPERTIES, false);
	CTimerManager::UpdateIntervalUI();
}

void CSystemInfoView::OnPageActivated(bool active) {
	UI().UIEnable(ID_RUN, true);
	ActivateTimer(active);
	if (active) {
		Refresh();
		GetFrame()->SetStatusText(7, L"");
	}
}

void CSystemInfoView::DoTimerUpdate() {
	Refresh();
}

LRESULT CSystemInfoView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.InsertColumn(0, L"Property", LVCFMT_LEFT, 200);
	m_List.InsertColumn(1, L"Value", LVCFMT_LEFT, 350);
	InitListLayout(m_List, L"SystemInformation");

	Refresh();
	Run(true);
	return 0;
}

LRESULT CSystemInfoView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List, L","));
	return 0;
}

LRESULT CSystemInfoView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}
