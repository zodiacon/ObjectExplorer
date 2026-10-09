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
	static std::vector<Property> GetProperties(HANDLE hObject, PCWSTR type);

private:
	struct TypeEntry {
		PCWSTR Type;
		PCWSTR Title;
		ACCESS_MASK QueryAccess;
		std::vector<Property>(*GetProperties)(HANDLE);
	};
	static TypeEntry const* FindType(PCWSTR type);
	static wil::unique_handle DuplicateForQuery(HANDLE hObject, ACCESS_MASK access);

	static std::vector<Property> GetEventProperties(HANDLE hEvent);
	static std::vector<Property> GetMutantProperties(HANDLE hMutant);
	static std::vector<Property> GetSemaphoreProperties(HANDLE hSemaphore);
	static std::vector<Property> GetTimerProperties(HANDLE hTimer);
	static std::vector<Property> GetSectionProperties(HANDLE hSection);
	static std::vector<Property> GetProcessProperties(HANDLE hProcess);
	static std::vector<Property> GetThreadProperties(HANDLE hThread);
	static std::vector<Property> GetJobProperties(HANDLE hJob);
};
