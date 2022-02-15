#pragma once

struct ProcessHelper abstract final {
	static CString GetProcessName(DWORD pid);

private:
	static void EnumProcesses(bool force = false);
	inline static std::unordered_map<DWORD, CString> s_names;
};

