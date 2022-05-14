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

CString ProcessHelper::GetProcessName2(DWORD pid) {
	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
	if (hProcess) {
		WCHAR name[MAX_PATH];
		if (::GetProcessImageFileName(hProcess.get(), name, _countof(name))) {
			return wcsrchr(name, L'\\') + 1;
		}
	}
	EnumProcesses();
	if (auto it = s_names.find(pid); it != s_names.end())
		return it->second;
	return L"<Unknown>";
}

std::wstring ProcessHelper::GetUserName(DWORD pid) {
	if (pid <= 4)
		return L"NT AUTHORITY\\System";

	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid));
	if (!hProcess)
		return {};

	wil::unique_handle hToken;
	if (!::OpenProcessToken(hProcess.get(), TOKEN_QUERY, hToken.addressof()))
		return L"";

	BYTE buffer[256];
	DWORD len;
	if (!::GetTokenInformation(hToken.get(), TokenUser, buffer, sizeof(buffer), &len))
		return L"";

	auto user = reinterpret_cast<TOKEN_USER*>(buffer);
	DWORD userMax = TOKEN_USER_MAX_SIZE;
	wchar_t name[TOKEN_USER_MAX_SIZE];
	DWORD domainMax = 64;
	wchar_t domain[64];
	SID_NAME_USE use;
	if (!::LookupAccountSid(nullptr, user->User.Sid, name, &userMax, domain, &domainMax, &use))
		return L"";

	return std::wstring(domain) + L"\\" + name;
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

std::wstring ProcessHelper::GetDosNameFromNtName(PCWSTR name) {
	static std::vector<std::pair<std::wstring, std::wstring>> deviceNames;
	static bool first = true;
	if (first) {
		auto drives = ::GetLogicalDrives();
		int drive = 0;
		while (drives) {
			if (drives & 1) {
				// drive exists
				WCHAR driveName[] = L"X:";
				driveName[0] = (WCHAR)(drive + 'A');
				WCHAR path[MAX_PATH];
				if (::QueryDosDevice(driveName, path, MAX_PATH)) {
					deviceNames.push_back({ path, driveName });
				}
			}
			drive++;
			drives >>= 1;
		}
		first = false;
	}

	for (auto& [ntName, dosName] : deviceNames) {
		if (::_wcsnicmp(name, ntName.c_str(), ntName.size()) == 0)
			return dosName + (name + ntName.size());
	}
	return L"";
}
