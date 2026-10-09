#pragma once

//
// type-specific properties of an object (e.g. an event's type and state), shown in a page of the
// properties dialog named after the type; the General page shows the information common to all types
//
struct TypeProperties final {
	struct Property {
		CString Name;
		CString Value;
	};

	// whether the type has a page of its own
	static bool HasProperties(PCWSTR type);
	// the page's title (e.g. "Mutex" for a Mutant)
	static CString GetPageTitle(PCWSTR type);
	// hObject is a handle in this process (typically with READ_CONTROL access only); it's duplicated with the
	// access the queries need, through the driver if necessary; properties that can't be queried are omitted
	// pid is the process the handle was duplicated from (0 if opened by this process); window stations and
	// desktops of other sessions aren't queried
	static std::vector<Property> GetProperties(HANDLE hObject, PCWSTR type, DWORD pid = 0);

private:
	struct TypeEntry {
		PCWSTR Type;
		PCWSTR Title;
		ACCESS_MASK QueryAccess;	// 0 to query with the handle as is
		std::vector<Property>(*GetProperties)(HANDLE);
		bool SessionObject{ false };	// lives in session space
		// for types whose handles can't be duplicated with more access; opens the object again with the access
		HANDLE(*Reopen)(HANDLE, ACCESS_MASK){ nullptr };
	};
	static TypeEntry const* FindType(PCWSTR type);
	static HANDLE ReopenKey(HANDLE hKey, ACCESS_MASK access);
	static HANDLE ReopenFile(HANDLE hFile, ACCESS_MASK access);
	static DWORD GetObjectSession(HANDLE hObject, PCWSTR type, DWORD pid);
	static wil::unique_handle DuplicateForQuery(HANDLE hObject, TypeEntry const& entry);

	static std::vector<Property> GetEventProperties(HANDLE hEvent);
	static std::vector<Property> GetMutantProperties(HANDLE hMutant);
	static std::vector<Property> GetSemaphoreProperties(HANDLE hSemaphore);
	static std::vector<Property> GetTimerProperties(HANDLE hTimer);
	static std::vector<Property> GetSectionProperties(HANDLE hSection);
	static std::vector<Property> GetProcessProperties(HANDLE hProcess);
	static std::vector<Property> GetThreadProperties(HANDLE hThread);
	static std::vector<Property> GetJobProperties(HANDLE hJob);
	static std::vector<Property> GetWindowStationProperties(HANDLE hWinSta);
	static std::vector<Property> GetDesktopProperties(HANDLE hDesktop);
	static std::vector<Property> GetKeyProperties(HANDLE hKey);
	static std::vector<Property> GetAlpcPortProperties(HANDLE hPort);
	static std::vector<Property> GetTokenProperties(HANDLE hToken);
	static std::vector<Property> GetSessionProperties(HANDLE hSession);
	static std::vector<Property> GetFileProperties(HANDLE hFile);
};
