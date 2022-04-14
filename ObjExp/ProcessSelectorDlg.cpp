#include "pch.h"
#include "resource.h"
#include "ProcessSelectorDlg.h"
#include <TlHelp32.h>
#include "ProcessHelper.h"
#include "SortHelper.h"

DWORD CProcessSelectorDlg::GetSelectedProcess() const {
    return m_SelectedPid;
}

CString CProcessSelectorDlg::GetColumnText(HWND, int row, int col) const {
    auto& item = m_Processes[row];

    switch (col) {
        case 0: return item.Name.c_str();
        case 1: return std::to_wstring(item.Id).c_str();
        case 2: return std::to_wstring(item.Session).c_str();
        case 3: return item.UserName.c_str();
    }
    return CString();
}

int CProcessSelectorDlg::GetRowImage(HWND, int row, int col) const {
    auto& item = m_Processes[row];
    if (item.Image < 0) {
        wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, item.Id));
        HICON hIcon = nullptr;
        WCHAR path[MAX_PATH];
        if (hProcess) {
            DWORD size = _countof(path);
            if (::QueryFullProcessImageName(hProcess.get(), 0, path, &size)) {
                if (auto it = m_IconsMap.find(path); it != m_IconsMap.end())
                    item.Image = it->second;
                else
                    ::ExtractIconEx(path, 0, nullptr, &hIcon, 1);
            }
        }
        if (item.Image < 0) {
            if (hIcon) {
                item.Image = m_Icons.AddIcon(hIcon);
                m_IconsMap.insert({ path, item.Image });
            }
            else
                item.Image = 0;
        }
    }
    return item.Image;
}

void CProcessSelectorDlg::DoSort(SortInfo const* si) {
    if (si == nullptr)
        return;

    auto compare = [&](auto& p1, auto& p2) {
        switch (si->SortColumn) {
            case 0: return SortHelper::Sort(p1.Name, p2.Name, si->SortAscending);
            case 1: return SortHelper::Sort(p1.Id, p2.Id, si->SortAscending);
            case 2: return SortHelper::Sort(p1.Session, p2.Session, si->SortAscending);
            case 3: return SortHelper::Sort(p1.UserName, p2.UserName, si->SortAscending);
        }
        return false;
    };
    m_Processes.Sort(compare);
    m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
}

void CProcessSelectorDlg::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
    GetDlgItem(IDOK).EnableWindow(m_List.GetSelectedIndex() >= 0);
}

bool CProcessSelectorDlg::OnDoubleClickList(HWND, int row, int col, CPoint const&) {
    if (row >= 0) {
        m_SelectedPid = m_Processes[row].Id;
        EndDialog(IDOK);
    }
    return true;
}

LRESULT CProcessSelectorDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();
    SetDialogIcon(IDI_PROCESS);
    m_QuickFind.SubclassWindow(GetDlgItem(IDC_TEXT));
    m_QuickFind.SetWatermark(L"Type to filter");
    m_QuickFind.SetWatermarkIcon(AtlLoadIconImage(IDI_SEARCH, 0, 16, 16));
	m_List.Attach(GetDlgItem(IDC_LIST));
	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
    m_Icons.Create(16, 16, ILC_COLOR32 | ILC_MASK, 64, 32);
    m_Icons.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
    m_List.SetImageList(m_Icons, LVSIL_SMALL);

    m_List.InsertColumn(0, L"Name", LVCFMT_LEFT, 240);
    m_List.InsertColumn(1, L"ID", LVCFMT_RIGHT, 70);
    m_List.InsertColumn(2, L"Session", LVCFMT_RIGHT, 60);
    m_List.InsertColumn(3, L"User Name", LVCFMT_LEFT, 200);

	InitProcesses();
    m_List.SetItemCount((int)m_Processes.size());
    ::RegisterHotKey(m_hWnd, 1, MOD_CONTROL, 'Q');

	return 0;
}

LRESULT CProcessSelectorDlg::OnCloseCmd(WORD, WORD wID, HWND, BOOL&) {
    if (wID == IDOK)
        m_SelectedPid = m_Processes[m_List.GetSelectedIndex()].Id;
    ::UnregisterHotKey(m_hWnd, 1);
    EndDialog(wID);
	return 0;
}

LRESULT CProcessSelectorDlg::OnHotKey(UINT, WPARAM, LPARAM, BOOL&) {
    m_QuickFind.SetFocus();
    return 0;
}

LRESULT CProcessSelectorDlg::OnFilterChanged(WORD, WORD id, HWND, BOOL&) {
    CString text;
    m_QuickFind.GetWindowText(text);
    ApplyFilter(text);
    m_List.SetItemCountEx((int)m_Processes.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
    m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());

    return 0;
}

LRESULT CProcessSelectorDlg::OnRefresh(WORD, WORD id, HWND, BOOL&) {
    InitProcesses();
    CString text;
    m_QuickFind.GetWindowText(text);
    if (!text.IsEmpty())
        ApplyFilter(text);
    Sort(GetSortInfo(m_List));
    m_List.SetItemCountEx((int)m_Processes.size(), LVSICF_NOSCROLL | LVSICF_NOINVALIDATEALL);
    m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());

    return 0;
}

void CProcessSelectorDlg::ApplyFilter(PCWSTR filter) {
    CString text(filter);
    if (text.IsEmpty())
        m_Processes.Filter(nullptr);
    else {
        text.MakeLower();
        m_Processes.Filter([&](auto& p, int) {
            CString name(p.Name.c_str());
            name.MakeLower();
            return name.Find(text) >= 0;
            });
    }
}

void CProcessSelectorDlg::InitProcesses() {
    m_Processes.clear();
    m_Processes.reserve(512);
   
    wil::unique_handle hSnaphost(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnaphost)
        return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    ::Process32First(hSnaphost.get(), &pe);

    while (::Process32Next(hSnaphost.get(), &pe)) {
        ProcessInfo pi;
        pi.Name = pe.szExeFile;
        pi.Id = pe.th32ProcessID;
        ::ProcessIdToSessionId(pi.Id, &pi.Session);
        if (pi.Name.rfind(L'.') == std::wstring::npos) {
            pi.UserName = L"NT AUTHORITY\\System";
            pi.Image = 0;
        }
        else {
            pi.UserName = ProcessHelper::GetUserName(pi.Id);
            if (pi.UserName.empty())
                pi.UserName = L"<access denied>";
        }
        m_Processes.push_back(std::move(pi));
    }
}
