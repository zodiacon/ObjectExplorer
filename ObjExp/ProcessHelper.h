#pragma once

struct ProcessHelper abstract final {
	static CString GetProcessName(DWORD pid);
	static CString GetProcessName2(DWORD pid);
	static CString GetFullProcessImageName(DWORD pid);
	static std::wstring GetUserName(DWORD pid);
	static std::wstring GetDosNameFromNtName(PCWSTR name);

private:
	static void EnumProcesses(bool force = false);
	inline static std::unordered_map<DWORD, CString> s_names;
};

