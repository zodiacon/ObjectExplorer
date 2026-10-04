#pragma once

struct ProcessHelper abstract final {
	static CString GetProcessName(DWORD pid);
	static CString GetProcessName2(DWORD pid);
	static CString GetFullProcessImageName(DWORD pid);
	static std::wstring GetUserName(DWORD pid);
	static std::wstring GetDosNameFromNtName(PCWSTR name);

private:
	static CString LookupName(DWORD pid);
	static std::unordered_map<DWORD, CString> EnumProcesses();

	// name cache for processes that can't be opened; used from the UI thread and worker threads
	inline static wil::srwlock s_lock;
	inline static std::unordered_map<DWORD, CString> s_names;
	inline static DWORD64 s_lastEnum;
};

