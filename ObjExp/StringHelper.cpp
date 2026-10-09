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

CString StringHelper::ObjectAttributesToString(DWORD attr) {
	CString text;
	const struct {
		DWORD attribute;
		PCWSTR text;
	} attributes[] = {
		{ OBJ_KERNEL_HANDLE, L"KERNEL_HANDLE" },
		{ OBJ_OPENLINK, L"OPEN_LINK" },
		{ OBJ_OPENIF, L"OPEN_IF" },
		{ OBJ_PERMANENT, L"PERMANENT" },
		{ OBJ_EXCLUSIVE, L"EXCLUSIVE" },
		{ OBJ_INHERIT, L"INHERIT" },
		{ OBJ_CASE_INSENSITIVE, L"CASE_INSENSITIVE" },
		{ OBJ_FORCE_ACCESS_CHECK, L"FORCE_ACCESS_CHECK" },
		{ OBJ_IGNORE_IMPERSONATED_DEVICEMAP, L"IGNORE_IMPERSONATED_DEVICEMAP" },
		{ OBJ_DONT_REPARSE, L"DONT_REPARSE" },
	};

	for (auto const& item : attributes)
		if (attr & item.attribute)
			(text += item.text) += L", ";
	if(!text.IsEmpty())
		text = text.Left(text.GetLength() - 2);
	return text;
}

CString StringHelper::SidToName(PSID sid) {
	WCHAR name[128], domain[64];
	DWORD nameSize = _countof(name), domainSize = _countof(domain);
	SID_NAME_USE use;
	if (!::LookupAccountSid(nullptr, sid, name, &nameSize, domain, &domainSize, &use))
		return L"";
	return domain[0] ? CString(domain) + L"\\" + name : CString(name);
}

CString StringHelper::IntegrityLevelToString(DWORD rid) {
	if (rid >= SECURITY_MANDATORY_PROTECTED_PROCESS_RID)
		return L"Protected";
	if (rid >= SECURITY_MANDATORY_SYSTEM_RID)
		return L"System";
	if (rid >= SECURITY_MANDATORY_HIGH_RID)
		return L"High";
	if (rid > SECURITY_MANDATORY_MEDIUM_RID)
		return L"Medium+";
	if (rid == SECURITY_MANDATORY_MEDIUM_RID)
		return L"Medium";
	if (rid >= SECURITY_MANDATORY_LOW_RID)
		return L"Low";
	return L"Untrusted";
}

CString StringHelper::MachineToString(USHORT machine) {
	switch (machine) {
		case IMAGE_FILE_MACHINE_I386: return L"x86";
		case IMAGE_FILE_MACHINE_AMD64: return L"x64";
		case IMAGE_FILE_MACHINE_ARM64: return L"ARM64";
		case IMAGE_FILE_MACHINE_ARMNT: return L"ARM";
	}
	return std::format(L"0x{:04X}", machine).c_str();
}

PCWSTR StringHelper::DeviceTypeToString(ULONG type) {
	// FILE_DEVICE_* values (winioctl.h)
	switch (type) {
		case 0x02: return L"CD-ROM";
		case 0x03: return L"CD-ROM File System";
		case 0x07: return L"Disk";
		case 0x08: return L"Disk File System";
		case 0x0C: return L"Mailslot";
		case 0x11: return L"Named Pipe";
		case 0x12: return L"Network";
		case 0x14: return L"Network File System";
		case 0x15: return L"Null";
		case 0x2F: return L"Kernel Streaming";
		case 0x39: return L"KSecDD";
		case 0x50: return L"Console";
	}
	return nullptr;
}

CString StringHelper::FileAttributesToString(ULONG attributes) {
	static const std::pair<ULONG, PCWSTR> names[] = {
		{ FILE_ATTRIBUTE_READONLY, L"Read Only" },
		{ FILE_ATTRIBUTE_HIDDEN, L"Hidden" },
		{ FILE_ATTRIBUTE_SYSTEM, L"System" },
		{ FILE_ATTRIBUTE_ARCHIVE, L"Archive" },
		{ FILE_ATTRIBUTE_TEMPORARY, L"Temporary" },
		{ FILE_ATTRIBUTE_SPARSE_FILE, L"Sparse" },
		{ FILE_ATTRIBUTE_REPARSE_POINT, L"Reparse Point" },
		{ FILE_ATTRIBUTE_COMPRESSED, L"Compressed" },
		{ FILE_ATTRIBUTE_ENCRYPTED, L"Encrypted" },
		{ FILE_ATTRIBUTE_OFFLINE, L"Offline" },
	};
	CString text;
	for (auto& [value, name] : names) {
		if (attributes & value) {
			if (!text.IsEmpty())
				text += L" | ";
			text += name;
		}
	}
	return text;
}

PCWSTR StringHelper::PipeStateToString(ULONG state) {
	switch (state) {
		case 1: return L"Disconnected";
		case 2: return L"Listening";
		case 3: return L"Connected";
		case 4: return L"Closing";
	}
	return nullptr;
}
