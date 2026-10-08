#pragma once

//
// a one line summary of an object's state by its type (e.g. a process' ID, threads and user),
// for list views showing many objects; the properties dialog shows more
//
struct ObjectDetails abstract final {
	// hObject is a handle in this process; the information available depends on its access
	// key identifies the object for queries that may block (files); see FileQuery
	static CString GetDetails(HANDLE hObject, PCWSTR type, ULONG64 key = 0);
	// duplicates the handle from its process (with the same access) to get the details;
	// window stations and desktops are queried only for processes in this session
	static CString GetDetails(HANDLE hObject, DWORD pid, PCWSTR type, PVOID object = nullptr);

private:
	static CString GetProcessDetails(HANDLE hProcess);
	static CString GetThreadDetails(HANDLE hThread);
	static CString GetTokenDetails(HANDLE hToken);
	static CString GetKeyDetails(HANDLE hKey);
	static CString GetFileDetails(HANDLE hFile, ULONG64 key);
	static CString GetWindowStationDetails(HANDLE hWinSta);
	static CString GetDesktopDetails(HANDLE hDesktop);
	static DWORD GetThreadCount(DWORD pid);

	// thread counts from the last process snapshot; used from the UI thread and worker threads
	inline static wil::srwlock s_lock;
	inline static std::unordered_map<DWORD, DWORD> s_threadCounts;
	inline static DWORD64 s_lastSnapshot;
};
