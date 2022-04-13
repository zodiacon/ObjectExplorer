#include "pch.h"
#include "ProcessHelper.h"
#include <TlHelp32.h>

CString ProcessHelper::GetProcessName(DWORD pid) {
    wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (hProcess) {
        WCHAR name[MAX_PATH];
        DWORD size = _countof(name);
        if (::QueryFullProcessImageName(hProcess.get(), 0, name, &size)) {
            return wcsrchr(name, L'\\') + 1;
        }
    }
    EnumProcesses();
    if (auto it = s_names.find(pid); it != s_names.end())
        return it->second;
    return L"<Unknown>";
}

void ProcessHelper::EnumProcesses(bool force) {
    if (!force && !s_names.empty())
        return;

    wil::unique_handle hSnaphost(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
    if (!hSnaphost)
        return;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    ::Process32First(hSnaphost.get(), &pe);

    while (::Process32Next(hSnaphost.get(), &pe)) {
        wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID));
        if (hProcess && wcsrchr(pe.szExeFile, L'.'))
            continue;

        s_names.insert({ pe.th32ProcessID, pe.szExeFile });
    }
}
