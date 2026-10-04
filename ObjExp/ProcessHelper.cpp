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
	return LookupName(pid);
}

CString ProcessHelper::GetProcessName2(DWORD pid) {
	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
	if (hProcess) {
		WCHAR name[MAX_PATH];
		if (::GetProcessImageFileName(hProcess.get(), name, _countof(name))) {
			return wcsrchr(name, L'\\') + 1;
		}
	}
	return LookupName(pid);
}

CString ProcessHelper::LookupName(DWORD pid) {
	//
	// a snapshot older than this may miss new processes or have a reused PID's old name
	//
	const DWORD64 MaxAge = 5000;
	// don't take a new snapshot for every unknown PID (e.g. handles of processes that exited)
	const DWORD64 MinRefreshInterval = 1000;

	auto now = ::GetTickCount64();
	{
		auto lock = s_lock.lock_shared();
		auto age = now - s_lastEnum;
		auto it = s_names.find(pid);
		if (it != s_names.end() && age < MaxAge)
			return it->second;
		if (it == s_names.end() && age < MinRefreshInterval)
			return L"<Unknown>";
	}

	auto names = EnumProcesses();
	auto lock = s_lock.lock_exclusive();
	s_names = std::move(names);
	s_lastEnum = ::GetTickCount64();
	if (auto it = s_names.find(pid); it != s_names.end())
		return it->second;
	return L"<Unknown>";
}

CString ProcessHelper::GetFullProcessImageName(DWORD pid) {
	wil::unique_handle hProcess(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
	if (hProcess) {
		WCHAR name[MAX_PATH];
		DWORD size = _countof(name);
		if (::QueryFullProcessImageName(hProcess.get(), 0, name, &size)) {
			return name;
		}
	}
	return GetProcessName2(pid);
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

std::unordered_map<DWORD, CString> ProcessHelper::EnumProcesses() {
	std::unordered_map<DWORD, CString> names;
	wil::unique_handle hSnaphost(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (!hSnaphost)
		return names;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);
	if (!::Process32First(hSnaphost.get(), &pe))
		return names;

	names.reserve(512);
	do {
		names.insert({ pe.th32ProcessID, pe.szExeFile });
	} while (::Process32Next(hSnaphost.get(), &pe));
	return names;
}

std::wstring ProcessHelper::GetDosNameFromNtName(PCWSTR name) {
	static wil::srwlock lock;
	static std::vector<std::pair<std::wstring, std::wstring>> deviceNames;
	static DWORD64 lastUpdate;

	//
	// rebuild the drive list now and then, as drives can be added or removed (e.g. USB, mounted images)
	//
	auto now = ::GetTickCount64();
	auto guard = lock.lock_exclusive();
	if (deviceNames.empty() || now - lastUpdate > 5000) {
		deviceNames.clear();
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
		lastUpdate = now;
	}

	for (auto& [ntName, dosName] : deviceNames) {
		// the device name must be followed by a separator, or \Device\HarddiskVolume1 would match \Device\HarddiskVolume10
		auto len = ntName.size();
		if (::_wcsnicmp(name, ntName.c_str(), len) == 0 && (name[len] == L'\\' || name[len] == 0))
			return dosName + (name + len);
	}
	return L"";
}
