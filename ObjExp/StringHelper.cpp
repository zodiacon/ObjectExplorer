#include "pch.h"
#include "StringHelper.h"

PCWSTR StringHelper::PoolTypeToString(PoolType type) {
	switch (type) {
		case PoolType::NonPagedPool:
			return L"Non Paged";
		case PoolType::PagedPool:
			return L"Paged";
		case PoolType::NonPagedPoolNx:
			return L"Non Paged NX";
		case PoolType::PagedPoolSessionNx:
			return L"Paged Session NX";
	}
	return L"Unknown";
}

CString StringHelper::SectionAttributesToString(DWORD value) {
	CString text;
	const struct {
		DWORD attribute;
		PCWSTR text;
	} attributes[] = {
		{ SEC_COMMIT, L"Commit" },
		{ SEC_RESERVE, L"Reserve" },
		{ SEC_IMAGE, L"Image" },
		{ SEC_NOCACHE, L"No Cache" },
		{ SEC_FILE, L"File" },
		{ SEC_WRITECOMBINE, L"Write Combine" },
		{ SEC_PROTECTED_IMAGE, L"Protected Image" },
		{ SEC_LARGE_PAGES, L"Large Pages" },
		{ SEC_IMAGE_NO_EXECUTE, L"No Execute" },
	};

	for (auto const& item : attributes)
		if (value & item.attribute)
			(text += item.text) += L", ";
	if (text.GetLength() == 0)
		text = L"None";
	else
		text = text.Left(text.GetLength() - 2);
	return text;
}

CString StringHelper::HandleAttributesToString(DWORD attributes) {
	if (attributes == 0)
		return L"None (0)";
	CString text;
	if (attributes & OBJ_INHERIT)
		text += L"Inherit, ";
	if (attributes & 1)
		text += L"Protect, ";
	if (attributes & 4)
		text += L"Audit, ";
	return text.Left(text.GetLength() - 2) + L" (" + std::to_wstring(attributes).c_str() + L")";
}

CString StringHelper::TimeSpanToString(DWORD64 ts) {
	return CTimeSpan(ts / 10000000).Format(L"%H:%M:%S") + std::format(L".{:03}", ts / 10000 % 1000).c_str();
}
