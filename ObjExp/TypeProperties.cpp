#include "pch.h"
#include "TypeProperties.h"
#include "NtDll.h"
#include "ProcessHelper.h"
#include "DriverHelper.h"
#include "StringHelper.h"
#include <atltime.h>
#include <sddl.h>
#include <WtsApi32.h>
#include "FileQuery.h"

#pragma comment(lib, "wtsapi32")

namespace {
	// Windows 10 APIs, beyond the targeted version
	using IsWow64Process2Func = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
	using GetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PWSTR*);

	template<typename F>
	F GetKernel32Function(PCSTR name) {
		return reinterpret_cast<F>(::GetProcAddress(::GetModuleHandle(L"kernel32"), name));
	}

	CString FormatTime(FILETIME const& ft) {
		return CTime(ft).Format(L"%c");
	}

	CString FormatNumber(ULONGLONG value) {
		return std::format(L"{} (0x{:X})", value, value).c_str();
	}

	CString FormatSize(ULONGLONG bytes) {
		return std::format(L"{} bytes (0x{:X})", bytes, bytes).c_str();
	}

	CString FormatProcess(DWORD pid) {
		return std::format(L"{} (0x{:X}) {}", pid, pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str();
	}

	PCWSTR PriorityClassToString(DWORD priorityClass) {
		switch (priorityClass) {
			case IDLE_PRIORITY_CLASS: return L"Idle";
			case BELOW_NORMAL_PRIORITY_CLASS: return L"Below Normal";
			case NORMAL_PRIORITY_CLASS: return L"Normal";
			case ABOVE_NORMAL_PRIORITY_CLASS: return L"Above Normal";
			case HIGH_PRIORITY_CLASS: return L"High";
			case REALTIME_PRIORITY_CLASS: return L"Realtime";
		}
		return nullptr;
	}

	PCWSTR ThreadPriorityToString(int priority) {
		switch (priority) {
			case THREAD_PRIORITY_IDLE: return L"Idle";
			case THREAD_PRIORITY_LOWEST: return L"Lowest";
			case THREAD_PRIORITY_BELOW_NORMAL: return L"Below Normal";
			case THREAD_PRIORITY_NORMAL: return L"Normal";
			case THREAD_PRIORITY_ABOVE_NORMAL: return L"Above Normal";
			case THREAD_PRIORITY_HIGHEST: return L"Highest";
			case THREAD_PRIORITY_TIME_CRITICAL: return L"Time Critical";
		}
		return nullptr;
	}

	PCWSTR SubsystemToString(ULONG subsystem) {
		switch (subsystem) {
			case IMAGE_SUBSYSTEM_NATIVE: return L"Native";
			case IMAGE_SUBSYSTEM_WINDOWS_GUI: return L"Windows GUI";
			case IMAGE_SUBSYSTEM_WINDOWS_CUI: return L"Windows Console";
			case IMAGE_SUBSYSTEM_POSIX_CUI: return L"POSIX Console";
			case IMAGE_SUBSYSTEM_EFI_APPLICATION: return L"EFI Application";
			case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: return L"EFI Boot Service Driver";
			case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER: return L"EFI Runtime Driver";
		}
		return L"Unknown";
	}

	CString JobLimitsToString(DWORD flags) {
		static const std::pair<DWORD, PCWSTR> limits[] = {
			{ JOB_OBJECT_LIMIT_WORKINGSET, L"Working Set" },
			{ JOB_OBJECT_LIMIT_PROCESS_TIME, L"Process Time" },
			{ JOB_OBJECT_LIMIT_JOB_TIME, L"Job Time" },
			{ JOB_OBJECT_LIMIT_ACTIVE_PROCESS, L"Active Processes" },
			{ JOB_OBJECT_LIMIT_AFFINITY, L"Affinity" },
			{ JOB_OBJECT_LIMIT_PRIORITY_CLASS, L"Priority Class" },
			{ JOB_OBJECT_LIMIT_PRESERVE_JOB_TIME, L"Preserve Job Time" },
			{ JOB_OBJECT_LIMIT_SCHEDULING_CLASS, L"Scheduling Class" },
			{ JOB_OBJECT_LIMIT_PROCESS_MEMORY, L"Process Memory" },
			{ JOB_OBJECT_LIMIT_JOB_MEMORY, L"Job Memory" },
			{ JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION, L"Die on Unhandled Exception" },
			{ JOB_OBJECT_LIMIT_BREAKAWAY_OK, L"Breakaway OK" },
			{ JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK, L"Silent Breakaway OK" },
			{ JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, L"Kill on Job Close" },
			{ JOB_OBJECT_LIMIT_SUBSET_AFFINITY, L"Subset Affinity" },
		};
		CString text;
		for (auto& [flag, name] : limits) {
			if (flags & flag) {
				if (!text.IsEmpty())
					text += L", ";
				text += name;
			}
		}
		return text.IsEmpty() ? CString(L"None") : text;
	}

	void AddTokenProperties(std::vector<TypeProperties::Property>& props, HANDLE hProcess) {
		wil::unique_handle hToken;
		if (!::OpenProcessToken(hProcess, TOKEN_QUERY, hToken.addressof()))
			return;
		BYTE buffer[256];
		DWORD len;
		if (::GetTokenInformation(hToken.get(), TokenUser, buffer, sizeof(buffer), &len)) {
			if (auto name = StringHelper::SidToName(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid); !name.IsEmpty())
				props.push_back({ L"User", name });
		}
		if (::GetTokenInformation(hToken.get(), TokenIntegrityLevel, buffer, sizeof(buffer), &len)) {
			auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer)->Label.Sid;
			if (auto count = *::GetSidSubAuthorityCount(sid); count)
				props.push_back({ L"Integrity Level", StringHelper::IntegrityLevelToString(*::GetSidSubAuthority(sid, count - 1)) });
		}
		if (TOKEN_ELEVATION elevation; ::GetTokenInformation(hToken.get(), TokenElevation, &elevation, sizeof(elevation), &len))
			props.push_back({ L"Elevated", elevation.TokenIsElevated ? L"Yes" : L"No" });
	}

	CString FlagsToString(DWORD flags, std::initializer_list<std::pair<DWORD, PCWSTR>> names) {
		CString text;
		for (auto& [flag, name] : names) {
			if (flags & flag) {
				if (!text.IsEmpty())
					text += L", ";
				text += name;
			}
		}
		return text.IsEmpty() ? CString(L"None") : text;
	}

	CString GetUserObjectName(HANDLE hObject) {
		WCHAR name[256];
		DWORD len;
		return ::GetUserObjectInformation(hObject, UOI_NAME, name, sizeof(name), &len) ? CString(name) : CString();
	}

	void AddUserObjectSid(std::vector<TypeProperties::Property>& props, HANDLE hObject) {
		BYTE sid[SECURITY_MAX_SID_SIZE];
		DWORD len;
		if (::GetUserObjectInformation(hObject, UOI_USER_SID, sid, sizeof(sid), &len) && len) {
			auto name = StringHelper::SidToName((PSID)sid);
			if (name.IsEmpty()) {
				PWSTR ssid;
				if (::ConvertSidToStringSid((PSID)sid, &ssid)) {
					name = ssid;
					::LocalFree(ssid);
				}
			}
			props.push_back({ L"User", name });
		}
	}

	CString FormatLuid(LUID const& luid) {
		return std::format(L"0x{:X}:0x{:08X}", (ULONG)luid.HighPart, luid.LowPart).c_str();
	}

	PCWSTR ImpersonationLevelToString(SECURITY_IMPERSONATION_LEVEL level) {
		switch (level) {
			case SecurityAnonymous: return L"Anonymous";
			case SecurityIdentification: return L"Identification";
			case SecurityImpersonation: return L"Impersonation";
			case SecurityDelegation: return L"Delegation";
		}
		return L"Unknown";
	}

	// variable size token information
	std::unique_ptr<BYTE[]> GetTokenInfo(HANDLE hToken, TOKEN_INFORMATION_CLASS infoClass) {
		DWORD len = 0;
		::GetTokenInformation(hToken, infoClass, nullptr, 0, &len);
		if (len == 0)
			return nullptr;
		auto buffer = std::make_unique<BYTE[]>(len);
		return ::GetTokenInformation(hToken, infoClass, buffer.get(), len, &len) ? std::move(buffer) : nullptr;
	}

	CString SidToDisplayName(PSID sid) {
		auto name = StringHelper::SidToName(sid);
		if (name.IsEmpty()) {
			PWSTR ssid;
			if (::ConvertSidToStringSid(sid, &ssid)) {
				name = ssid;
				::LocalFree(ssid);
			}
		}
		return name;
	}

	PCWSTR ConnectStateToString(WTS_CONNECTSTATE_CLASS state) {
		switch (state) {
			case WTSActive: return L"Active";
			case WTSConnected: return L"Connected";
			case WTSConnectQuery: return L"Connect Query";
			case WTSShadow: return L"Shadow";
			case WTSDisconnected: return L"Disconnected";
			case WTSIdle: return L"Idle";
			case WTSListen: return L"Listen";
			case WTSReset: return L"Reset";
			case WTSDown: return L"Down";
			case WTSInit: return L"Init";
		}
		return L"Unknown";
	}

	//
	// collected on a FileQuery worker thread, as querying synchronous file objects may block
	//
	struct FileData {
		NT::FILE_FS_DEVICE_INFORMATION Device;
		NT::FILE_STANDARD_INFORMATION Standard;
		NT::FILE_BASIC_INFORMATION Basic;
		NT::FILE_PIPE_LOCAL_INFORMATION Pipe;
		NT::FILE_INTERNAL_INFORMATION Internal;
		NT::FILE_POSITION_INFORMATION Position;
		ULONG Mode;
		bool HasDevice, HasStandard, HasBasic, HasPipe, HasInternal, HasPosition, HasMode;
	};
	static_assert(sizeof(FileData) <= FileQuery::BufferSize);

	bool QueryFileData(HANDLE hFile, BYTE* buffer) {
		auto data = new (buffer) FileData{};
		IO_STATUS_BLOCK ioStatus;
		data->HasDevice = NT_SUCCESS(NT::NtQueryVolumeInformationFile(hFile, &ioStatus, &data->Device, sizeof(data->Device), NT::FileFsDeviceInformation));
		NT::FILE_MODE_INFORMATION mode;
		data->HasMode = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &mode, sizeof(mode), NT::FileModeInformation));
		data->Mode = mode.Mode;
		const ULONG NamedPipeDevice = 0x11;
		if (data->HasDevice && data->Device.DeviceType == NamedPipeDevice)
			data->HasPipe = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Pipe, sizeof(data->Pipe), NT::FilePipeLocalInformation));
		else {
			data->HasStandard = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Standard, sizeof(data->Standard), NT::FileStandardInformation));
			data->HasBasic = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Basic, sizeof(data->Basic), NT::FileBasicInformation));
			data->HasInternal = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Internal, sizeof(data->Internal), NT::FileInternalInformation));
		}
		// the current position is maintained for synchronous I/O only
		if (data->HasMode && (data->Mode & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)))
			data->HasPosition = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Position, sizeof(data->Position), NT::FilePositionInformation));
		return data->HasDevice || data->HasMode || data->HasStandard || data->HasBasic || data->HasPipe;
	}

	CString FormatFileTime(LARGE_INTEGER const& time) {
		return time.QuadPart ? FormatTime(*(FILETIME const*)&time) : CString(L"None");
	}

	// process and thread times
	void AddTimes(std::vector<TypeProperties::Property>& props, FILETIME const& create, FILETIME const& exit,
		FILETIME const& kernel, FILETIME const& user, bool exited) {
		if (create.dwHighDateTime)
			props.push_back({ L"Started", FormatTime(create) });
		if (exited)
			props.push_back({ L"Exited", FormatTime(exit) });
		auto kernelTime = *(ULONGLONG const*)&kernel, userTime = *(ULONGLONG const*)&user;
		props.push_back({ L"Kernel Time", StringHelper::TimeSpanToString(kernelTime) });
		props.push_back({ L"User Time", StringHelper::TimeSpanToString(userTime) });
		props.push_back({ L"CPU Time", StringHelper::TimeSpanToString(kernelTime + userTime) });
	}
}

TypeProperties::TypeEntry const* TypeProperties::FindType(PCWSTR type) {
	static const TypeEntry types[] = {
		{ L"Event", L"Event", EVENT_QUERY_STATE, GetEventProperties },
		{ L"Mutant", L"Mutex", MUTANT_QUERY_STATE, GetMutantProperties },
		{ L"Semaphore", L"Semaphore", SEMAPHORE_QUERY_STATE, GetSemaphoreProperties },
		{ L"Timer", L"Timer", TIMER_QUERY_STATE, GetTimerProperties },
		{ L"Section", L"Section", SECTION_QUERY, GetSectionProperties },
		{ L"Process", L"Process", PROCESS_QUERY_LIMITED_INFORMATION, GetProcessProperties },
		{ L"Thread", L"Thread", THREAD_QUERY_LIMITED_INFORMATION, GetThreadProperties },
		{ L"Job", L"Job", JOB_OBJECT_QUERY, GetJobProperties },
		{ L"WindowStation", L"Window Station", WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES, GetWindowStationProperties, true },
		{ L"Desktop", L"Desktop", DESKTOP_READOBJECTS | DESKTOP_ENUMERATE, GetDesktopProperties, true },
		{ L"Key", L"Key", KEY_QUERY_VALUE, GetKeyProperties, false, ReopenKey },
		{ L"ALPC Port", L"ALPC Port", 0, GetAlpcPortProperties },
		{ L"Token", L"Token", TOKEN_QUERY, GetTokenProperties },
		// queried by the session ID in its name
		{ L"Session", L"Session", 0, GetSessionProperties },
		// most file queries need no access; the handle's own access is used if it can't be duplicated with more
		{ L"File", L"File", FILE_READ_ATTRIBUTES, GetFileProperties },
	};
	for (auto& entry : types)
		if (::_wcsicmp(type, entry.Type) == 0)
			return &entry;
	return nullptr;
}

bool TypeProperties::HasProperties(PCWSTR type) {
	return FindType(type) != nullptr;
}

CString TypeProperties::GetPageTitle(PCWSTR type) {
	auto entry = FindType(type);
	return entry ? entry->Title : type;
}

std::vector<TypeProperties::Property> TypeProperties::GetProperties(HANDLE hObject, PCWSTR type, DWORD pid) {
	auto entry = FindType(type);
	if (!entry)
		return {};
	if (entry->SessionObject) {
		DWORD ourSession = 0;
		::ProcessIdToSessionId(::GetCurrentProcessId(), &ourSession);
		if (auto session = GetObjectSession(hObject, type, pid); session != ourSession) {
			return {
				{ L"Session", session == (DWORD)-1 ? CString(L"Unknown") : CString(std::to_wstring(session).c_str()) },
				{ L"Note", L"Objects of other sessions can't be queried" },
			};
		}
	}
	auto hQuery = DuplicateForQuery(hObject, *entry);
	return entry->GetProperties(hQuery ? hQuery.get() : hObject);
}

HANDLE TypeProperties::ReopenKey(HANDLE hKey, ACCESS_MASK access) {
	// an empty name relative to the key opens the key itself; the root's handle needs no access
	UNICODE_STRING empty{};
	OBJECT_ATTRIBUTES attr;
	InitializeObjectAttributes(&attr, &empty, 0, hKey, nullptr);
	HANDLE h;
	return NT_SUCCESS(NT::NtOpenKey(&h, access, &attr)) ? h : nullptr;
}

HANDLE TypeProperties::ReopenFile(HANDLE hFile, ACCESS_MASK access) {
	//
	// opening a file has side effects on some devices (e.g. a pipe's or a socket's), and may block on remote
	// file systems, so only local disk files are opened again (as ReOpenFile does); opens for attributes only
	// don't conflict with the file's sharing mode
	//
	NT::FILE_FS_DEVICE_INFORMATION device;
	IO_STATUS_BLOCK ioStatus;
	if (!NT_SUCCESS(NT::NtQueryVolumeInformationFile(hFile, &ioStatus, &device, sizeof(device), NT::FileFsDeviceInformation)))
		return nullptr;
	if ((device.DeviceType != FILE_DEVICE_DISK && device.DeviceType != FILE_DEVICE_DISK_FILE_SYSTEM) || (device.Characteristics & FILE_REMOTE_DEVICE))
		return nullptr;
	if (access & ~(FILE_READ_ATTRIBUTES | SYNCHRONIZE))
		return nullptr;

	UNICODE_STRING empty{};
	OBJECT_ATTRIBUTES attr;
	InitializeObjectAttributes(&attr, &empty, 0, hFile, nullptr);
	HANDLE h;
	return NT_SUCCESS(NT::NtOpenFile(&h, access, &attr, &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0)) ? h : nullptr;
}

DWORD TypeProperties::GetObjectSession(HANDLE hObject, PCWSTR type, DWORD pid) {
	if (::_wcsicmp(type, L"WindowStation") == 0) {
		// named \Sessions\<n>\Windows\WindowStations\<name>, or \Windows\WindowStations\<name> in session 0
		BYTE buffer[1024];
		if (NT_SUCCESS(NT::NtQueryObject(hObject, NT::ObjectNameInformation, buffer, sizeof(buffer), nullptr))) {
			auto& name = reinterpret_cast<NT::OBJECT_NAME_INFORMATION*>(buffer)->Name;
			CString path(name.Buffer, name.Length / sizeof(WCHAR));
			if (path.Left(10).CompareNoCase(L"\\Sessions\\") == 0)
				return wcstoul(path.Mid(10), nullptr, 10);
			if (path.Left(9).CompareNoCase(L"\\Windows\\") == 0)
				return 0;
		}
	}
	DWORD session;
	if (pid == 0)
		pid = ::GetCurrentProcessId();
	return ::ProcessIdToSessionId(pid, &session) ? session : (DWORD)-1;
}

wil::unique_handle TypeProperties::DuplicateForQuery(HANDLE hObject, TypeEntry const& entry) {
	auto access = entry.QueryAccess;
	if (access == 0)
		return {};
	// for most types, access beyond the handle's is checked against the object's security descriptor;
	// keys and files don't allow more access than the handle has
	HANDLE hDup;
	if (::DuplicateHandle(::GetCurrentProcess(), hObject, ::GetCurrentProcess(), &hDup, access, FALSE, 0))
		return wil::unique_handle(hDup);
	// the driver duplicates in kernel mode, without the access check (requires elevation)
	if (hDup = DriverHelper::DupHandle(hObject, ::GetCurrentProcessId(), access, 0); hDup)
		return wil::unique_handle(hDup);
	return wil::unique_handle(entry.Reopen ? entry.Reopen(hObject, access) : nullptr);
}

std::vector<TypeProperties::Property> TypeProperties::GetEventProperties(HANDLE hEvent) {
	std::vector<Property> props;
	NT::EVENT_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtQueryEvent(hEvent, NT::EventBasicInformation, &info, sizeof(info), nullptr))) {
		props.push_back({ L"Event Type", info.EventType == NT::NotificationEvent ? L"Notification (Manual Reset)" : L"Synchronization (Auto Reset)" });
		props.push_back({ L"Signaled", info.EventState ? L"Yes" : L"No" });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetMutantProperties(HANDLE hMutant) {
	std::vector<Property> props;
	NT::MUTANT_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtQueryMutant(hMutant, NT::MutantBasicInformation, &info, sizeof(info), nullptr))) {
		// the count is 1 when free, and decremented with each (recursive) acquisition
		bool held = info.CurrentCount <= 0;
		props.push_back({ L"Signaled", held ? L"No" : L"Yes" });
		props.push_back({ L"Held", held ? L"Yes" : L"No" });
		if (held)
			props.push_back({ L"Recursion Count", std::to_wstring(1 - info.CurrentCount).c_str() });
		props.push_back({ L"Abandoned", info.AbandonedState ? L"Yes" : L"No" });
	}
	NT::MUTANT_OWNER_INFORMATION owner;
	if (NT_SUCCESS(NT::NtQueryMutant(hMutant, NT::MutantOwnerInformation, &owner, sizeof(owner), nullptr)) && owner.ClientId.UniqueThread) {
		auto pid = HandleToUlong(owner.ClientId.UniqueProcess);
		auto tid = HandleToUlong(owner.ClientId.UniqueThread);
		props.push_back({ L"Owner Process", FormatProcess(pid) });
		props.push_back({ L"Owner Thread", FormatNumber(tid) });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetSemaphoreProperties(HANDLE hSemaphore) {
	std::vector<Property> props;
	NT::SEMAPHORE_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtQuerySemaphore(hSemaphore, NT::SemaphoreBasicInformation, &info, sizeof(info), nullptr))) {
		props.push_back({ L"Signaled", info.CurrentCount > 0 ? L"Yes" : L"No" });
		props.push_back({ L"Count", FormatNumber(info.CurrentCount) });
		props.push_back({ L"Maximum Count", FormatNumber(info.MaximumCount) });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetTimerProperties(HANDLE hTimer) {
	std::vector<Property> props;
	NT::TIMER_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtQueryTimer(hTimer, NT::TimerBasicInformation, &info, sizeof(info), nullptr))) {
		props.push_back({ L"Signaled", info.TimerState ? L"Yes" : L"No" });
		// in 100 nsec units; not meaningful once the timer expired
		if (!info.TimerState && info.RemainingTime.QuadPart > 0)
			props.push_back({ L"Remaining Time", StringHelper::TimeSpanToString(info.RemainingTime.QuadPart) });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetSectionProperties(HANDLE hSection) {
	std::vector<Property> props;
	NT::SECTION_BASIC_INFORMATION info;
	if (!NT_SUCCESS(NT::NtQuerySection(hSection, NT::SectionBasicInformation, &info, sizeof(info), nullptr)))
		return props;

	props.push_back({ L"Size", FormatSize(info.MaximumSize.QuadPart) });
	props.push_back({ L"Attributes", std::format(L"0x{:08X} ({})", info.AllocationAttributes,
		(PCWSTR)StringHelper::SectionAttributesToString(info.AllocationAttributes)).c_str() });
	if (info.BaseAddress)
		props.push_back({ L"Base Address", std::format(L"0x{:X}", (ULONG_PTR)info.BaseAddress).c_str() });

	NT::SECTION_IMAGE_INFORMATION image;
	if ((info.AllocationAttributes & SEC_IMAGE) &&
		NT_SUCCESS(NT::NtQuerySection(hSection, NT::SectionImageInformation, &image, sizeof(image), nullptr))) {
		props.push_back({ L"Machine", StringHelper::MachineToString(image.Machine) });
		props.push_back({ L"Entry Point", std::format(L"0x{:X}", (ULONG_PTR)image.TransferAddress).c_str() });
		props.push_back({ L"Subsystem", std::format(L"{} ({}.{})", SubsystemToString(image.SubSystemType),
			image.SubSystemMajorVersion, image.SubSystemMinorVersion).c_str() });
		props.push_back({ L"Image File Size", FormatSize(image.ImageFileSize) });
		props.push_back({ L"Stack Reserve", FormatSize(image.MaximumStackSize) });
		props.push_back({ L"Stack Commit", FormatSize(image.CommittedStackSize) });
		props.push_back({ L"Characteristics", std::format(L"0x{:04X}", image.ImageCharacteristics).c_str() });
		props.push_back({ L"DLL Characteristics", std::format(L"0x{:04X}", image.DllCharacteristics).c_str() });
		props.push_back({ L"Contains Code", image.ImageContainsCode ? L"Yes" : L"No" });
		props.push_back({ L"Checksum", std::format(L"0x{:08X}", image.CheckSum).c_str() });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetProcessProperties(HANDLE hProcess) {
	std::vector<Property> props;
	auto pid = ::GetProcessId(hProcess);
	if (pid == 0)
		return props;

	props.push_back({ L"Process ID", FormatNumber(pid) });
	props.push_back({ L"Image Name", ProcessHelper::GetProcessName(pid) });
	WCHAR path[MAX_PATH * 2];
	if (DWORD size = _countof(path); ::QueryFullProcessImageName(hProcess, 0, path, &size))
		props.push_back({ L"Image Path", path });

	ULONG len = 0;
	NT::NtQueryInformationProcess(hProcess, ProcessCommandLineInformation, nullptr, 0, &len);
	if (len) {
		auto buffer = std::make_unique<BYTE[]>(len);
		if (NT_SUCCESS(NT::NtQueryInformationProcess(hProcess, ProcessCommandLineInformation, buffer.get(), len, &len))) {
			auto cmdline = reinterpret_cast<UNICODE_STRING*>(buffer.get());
			props.push_back({ L"Command Line", CString(cmdline->Buffer, cmdline->Length / sizeof(WCHAR)) });
		}
	}

	PROCESS_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtQueryInformationProcess(hProcess, ProcessBasicInformation, &info, sizeof(info), nullptr)))
		props.push_back({ L"Parent", FormatProcess(HandleToUlong(info.InheritedFromUniqueProcessId)) });

	DWORD code;
	bool exited = ::GetExitCodeProcess(hProcess, &code) && code != STILL_ACTIVE;
	if (exited)
		props.push_back({ L"Exit Code", std::format(L"0x{:X}", code).c_str() });

	if (DWORD session; ::ProcessIdToSessionId(pid, &session))
		props.push_back({ L"Session", std::to_wstring(session).c_str() });
	AddTokenProperties(props, hProcess);

	static const auto pIsWow64Process2 = GetKernel32Function<IsWow64Process2Func>("IsWow64Process2");
	if (USHORT processMachine, nativeMachine; pIsWow64Process2 && pIsWow64Process2(hProcess, &processMachine, &nativeMachine)) {
		props.push_back({ L"Architecture", processMachine == IMAGE_FILE_MACHINE_UNKNOWN ? StringHelper::MachineToString(nativeMachine)
			: StringHelper::MachineToString(processMachine) + L" (WOW64)" });
	}
	if (!exited) {
		if (auto priority = PriorityClassToString(::GetPriorityClass(hProcess)); priority)
			props.push_back({ L"Priority Class", priority });
		if (DWORD handles; ::GetProcessHandleCount(hProcess, &handles))
			props.push_back({ L"Handles", std::to_wstring(handles).c_str() });
	}
	if (FILETIME create, exit, kernel, user; ::GetProcessTimes(hProcess, &create, &exit, &kernel, &user))
		AddTimes(props, create, exit, kernel, user, exited);
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetThreadProperties(HANDLE hThread) {
	std::vector<Property> props;
	NT::THREAD_BASIC_INFORMATION info;
	if (!NT_SUCCESS(NT::NtQueryInformationThread(hThread, NT::ThreadBasicInformation, &info, sizeof(info), nullptr)))
		return props;

	props.push_back({ L"Thread ID", FormatNumber(HandleToUlong(info.ClientId.UniqueThread)) });
	props.push_back({ L"Process", FormatProcess(HandleToUlong(info.ClientId.UniqueProcess)) });
	static const auto pGetThreadDescription = GetKernel32Function<GetThreadDescriptionFunc>("GetThreadDescription");
	if (PWSTR desc; pGetThreadDescription && SUCCEEDED(pGetThreadDescription(hThread, &desc))) {
		if (*desc)
			props.push_back({ L"Description", desc });
		::LocalFree(desc);
	}

	bool exited = (DWORD)info.ExitStatus != STILL_ACTIVE;
	if (exited)
		props.push_back({ L"Exit Code", std::format(L"0x{:X}", (DWORD)info.ExitStatus).c_str() });
	else {
		props.push_back({ L"Dynamic Priority", std::to_wstring(info.Priority).c_str() });
		if (auto priority = ThreadPriorityToString(::GetThreadPriority(hThread)); priority)
			props.push_back({ L"Relative Priority", priority });
		props.push_back({ L"Affinity", std::format(L"0x{:X}", info.AffinityMask).c_str() });
	}
	if (info.TebBaseAddress)
		props.push_back({ L"TEB Address", std::format(L"0x{:X}", (ULONG_PTR)info.TebBaseAddress).c_str() });
	if (FILETIME create, exit, kernel, user; ::GetThreadTimes(hThread, &create, &exit, &kernel, &user))
		AddTimes(props, create, exit, kernel, user, exited);
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetJobProperties(HANDLE hJob) {
	std::vector<Property> props;
	JOBOBJECT_BASIC_AND_IO_ACCOUNTING_INFORMATION info;
	if (!::QueryInformationJobObject(hJob, JobObjectBasicAndIoAccountingInformation, &info, sizeof(info), nullptr))
		return props;

	auto& basic = info.BasicInfo;
	props.push_back({ L"Active Processes", std::to_wstring(basic.ActiveProcesses).c_str() });
	props.push_back({ L"Total Processes", std::to_wstring(basic.TotalProcesses).c_str() });
	props.push_back({ L"Terminated Processes", std::to_wstring(basic.TotalTerminatedProcesses).c_str() });

	// the active processes; fails if there are more than fit
	const DWORD maxCount = 1024;
	const DWORD size = sizeof(JOBOBJECT_BASIC_PROCESS_ID_LIST) + maxCount * sizeof(ULONG_PTR);
	auto buffer = std::make_unique<BYTE[]>(size);
	auto list = reinterpret_cast<JOBOBJECT_BASIC_PROCESS_ID_LIST*>(buffer.get());
	if (::QueryInformationJobObject(hJob, JobObjectBasicProcessIdList, list, size, nullptr) && list->NumberOfProcessIdsInList) {
		CString text;
		for (DWORD i = 0; i < list->NumberOfProcessIdsInList; i++) {
			auto pid = (DWORD)list->ProcessIdList[i];
			if (i)
				text += L", ";
			text += std::format(L"{} ({})", pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str();
		}
		props.push_back({ L"Processes", text });
	}

	props.push_back({ L"Kernel Time", StringHelper::TimeSpanToString(basic.TotalKernelTime.QuadPart) });
	props.push_back({ L"User Time", StringHelper::TimeSpanToString(basic.TotalUserTime.QuadPart) });
	props.push_back({ L"CPU Time", StringHelper::TimeSpanToString(basic.TotalKernelTime.QuadPart + basic.TotalUserTime.QuadPart) });
	props.push_back({ L"Page Faults", std::to_wstring(basic.TotalPageFaultCount).c_str() });
	auto& io = info.IoInfo;
	props.push_back({ L"I/O Reads", std::format(L"{} ({} bytes)", io.ReadOperationCount, io.ReadTransferCount).c_str() });
	props.push_back({ L"I/O Writes", std::format(L"{} ({} bytes)", io.WriteOperationCount, io.WriteTransferCount).c_str() });

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
	if (::QueryInformationJobObject(hJob, JobObjectExtendedLimitInformation, &limits, sizeof(limits), nullptr)) {
		auto flags = limits.BasicLimitInformation.LimitFlags;
		props.push_back({ L"Limits", JobLimitsToString(flags) });
		if (flags & JOB_OBJECT_LIMIT_ACTIVE_PROCESS)
			props.push_back({ L"Active Process Limit", std::to_wstring(limits.BasicLimitInformation.ActiveProcessLimit).c_str() });
		if (flags & JOB_OBJECT_LIMIT_PROCESS_MEMORY)
			props.push_back({ L"Process Memory Limit", FormatSize(limits.ProcessMemoryLimit) });
		if (flags & JOB_OBJECT_LIMIT_JOB_MEMORY)
			props.push_back({ L"Job Memory Limit", FormatSize(limits.JobMemoryLimit) });
		props.push_back({ L"Peak Process Memory", FormatSize(limits.PeakProcessMemoryUsed) });
		props.push_back({ L"Peak Job Memory", FormatSize(limits.PeakJobMemoryUsed) });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetWindowStationProperties(HANDLE hWinSta) {
	std::vector<Property> props;
	if (auto name = GetUserObjectName(hWinSta); !name.IsEmpty())
		props.push_back({ L"Name", name });
	DWORD len;
	if (USEROBJECTFLAGS flags; ::GetUserObjectInformation(hWinSta, UOI_FLAGS, &flags, sizeof(flags), &len))
		props.push_back({ L"Interactive", (flags.dwFlags & WSF_VISIBLE) ? L"Yes" : L"No" });
	AddUserObjectSid(props, hWinSta);

	CString desktops;
	if (::EnumDesktops((HWINSTA)hWinSta, [](auto name, auto param) {
		auto& desktops = *(CString*)param;
		if (!desktops.IsEmpty())
			desktops += L", ";
		desktops += name;
		return TRUE;
		}, (LPARAM)&desktops))
		props.push_back({ L"Desktops", desktops.IsEmpty() ? CString(L"None") : desktops });
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetDesktopProperties(HANDLE hDesktop) {
	std::vector<Property> props;
	if (auto name = GetUserObjectName(hDesktop); !name.IsEmpty())
		props.push_back({ L"Name", name });
	DWORD len;
	if (BOOL input; ::GetUserObjectInformation(hDesktop, UOI_IO, &input, sizeof(input), &len))
		props.push_back({ L"Receives Input", input ? L"Yes" : L"No" });
	if (ULONG heap; ::GetUserObjectInformation(hDesktop, UOI_HEAPSIZE, &heap, sizeof(heap), &len))
		props.push_back({ L"Heap Size", std::format(L"{} KB", heap).c_str() });
	if (USEROBJECTFLAGS flags; ::GetUserObjectInformation(hDesktop, UOI_FLAGS, &flags, sizeof(flags), &len))
		props.push_back({ L"Hooks of Other Accounts", (flags.dwFlags & DF_ALLOWOTHERACCOUNTHOOK) ? L"Allowed" : L"Not Allowed" });
	AddUserObjectSid(props, hDesktop);
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetKeyProperties(HANDLE hKey) {
	std::vector<Property> props;
	WCHAR className[256];
	DWORD classLen = _countof(className), subkeys, maxSubkeyLen, values, maxValueNameLen, maxValueLen;
	FILETIME lastWrite;
	if (::RegQueryInfoKey((HKEY)hKey, className, &classLen, nullptr, &subkeys, &maxSubkeyLen, nullptr,
		&values, &maxValueNameLen, &maxValueLen, nullptr, &lastWrite) == ERROR_SUCCESS) {
		props.push_back({ L"Subkeys", std::to_wstring(subkeys).c_str() });
		props.push_back({ L"Values", std::to_wstring(values).c_str() });
		props.push_back({ L"Last Write", FormatTime(lastWrite) });
		if (classLen)
			props.push_back({ L"Class", className });
		props.push_back({ L"Longest Subkey Name", std::format(L"{} characters", maxSubkeyLen).c_str() });
		props.push_back({ L"Longest Value Name", std::format(L"{} characters", maxValueNameLen).c_str() });
		props.push_back({ L"Largest Value Data", FormatSize(maxValueLen) });
	}

	NT::KEY_FLAGS_INFORMATION flags;
	ULONG len;
	if (NT_SUCCESS(NT::NtQueryKey(hKey, NT::KeyFlagsInformation, &flags, sizeof(flags), &len)) && len >= sizeof(flags)) {
		props.push_back({ L"Volatile", (flags.KeyFlags & REG_FLAG_VOLATILE) ? L"Yes" : L"No" });
		props.push_back({ L"Symbolic Link", (flags.KeyFlags & REG_FLAG_LINK) ? L"Yes" : L"No" });
	}
	NT::KEY_VIRTUALIZATION_INFORMATION virt;
	if (NT_SUCCESS(NT::NtQueryKey(hKey, NT::KeyVirtualizationInformation, &virt, sizeof(virt), &len))) {
		CString text;
		auto add = [&](bool set, PCWSTR name) {
			if (set) {
				if (!text.IsEmpty())
					text += L", ";
				text += name;
			}
		};
		add(virt.VirtualizationCandidate, L"Candidate");
		add(virt.VirtualizationEnabled, L"Enabled");
		add(virt.VirtualTarget, L"Virtual Target");
		add(virt.VirtualStore, L"Virtual Store");
		add(virt.VirtualSource, L"Virtualized");
		props.push_back({ L"Virtualization", text.IsEmpty() ? CString(L"None") : text });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetAlpcPortProperties(HANDLE hPort) {
	std::vector<Property> props;
	NT::ALPC_BASIC_INFORMATION info;
	if (NT_SUCCESS(NT::NtAlpcQueryInformation(hPort, NT::AlpcBasicInformation, &info, sizeof(info), nullptr))) {
		props.push_back({ L"Flags", std::format(L"0x{:08X} ({})", info.Flags, (PCWSTR)FlagsToString(info.Flags, {
			{ 0x10000, L"Allow Impersonation" },
			{ 0x20000, L"Allow LPC Requests" },
			{ 0x40000, L"Waitable" },
			{ 0x80000, L"Allow Duplicate Objects" },
			{ 0x100000, L"System Process" },
			{ 0x1000000, L"Direct Message" },
			{ 0x2000000, L"Allow Multi-Handle Attribute" },
		})).c_str() });
		props.push_back({ L"Sequence Number", std::to_wstring(info.SequenceNo).c_str() });
	}
	// for client communication ports
	NT::ALPC_SERVER_SESSION_INFORMATION session;
	if (NT_SUCCESS(NT::NtAlpcQueryInformation(hPort, NT::AlpcServerSessionInformation, &session, sizeof(session), nullptr))) {
		props.push_back({ L"Server Process", FormatProcess(session.ProcessId) });
		props.push_back({ L"Server Session", std::to_wstring(session.SessionId).c_str() });
	}
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetTokenProperties(HANDLE hToken) {
	std::vector<Property> props;
	DWORD len;
	if (auto user = GetTokenInfo(hToken, TokenUser); user)
		props.push_back({ L"User", SidToDisplayName(reinterpret_cast<TOKEN_USER*>(user.get())->User.Sid) });
	if (auto owner = GetTokenInfo(hToken, TokenOwner); owner)
		props.push_back({ L"Owner", SidToDisplayName(reinterpret_cast<TOKEN_OWNER*>(owner.get())->Owner) });

	TOKEN_STATISTICS stats;
	if (::GetTokenInformation(hToken, TokenStatistics, &stats, sizeof(stats), &len)) {
		if (stats.TokenType == TokenPrimary)
			props.push_back({ L"Type", L"Primary" });
		else
			props.push_back({ L"Type", std::format(L"Impersonation ({})", ImpersonationLevelToString(stats.ImpersonationLevel)).c_str() });
		props.push_back({ L"Token ID", FormatLuid(stats.TokenId) });
		props.push_back({ L"Logon Session", FormatLuid(stats.AuthenticationId) });
		props.push_back({ L"Modified ID", FormatLuid(stats.ModifiedId) });
	}
	if (DWORD session; ::GetTokenInformation(hToken, TokenSessionId, &session, sizeof(session), &len))
		props.push_back({ L"Session", std::to_wstring(session).c_str() });

	BYTE buffer[256];
	if (::GetTokenInformation(hToken, TokenIntegrityLevel, buffer, sizeof(buffer), &len)) {
		auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer)->Label.Sid;
		if (auto count = *::GetSidSubAuthorityCount(sid); count)
			props.push_back({ L"Integrity Level", StringHelper::IntegrityLevelToString(*::GetSidSubAuthority(sid, count - 1)) });
	}
	if (TOKEN_ELEVATION elevation; ::GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &len))
		props.push_back({ L"Elevated", elevation.TokenIsElevated ? L"Yes" : L"No" });
	if (TOKEN_ELEVATION_TYPE type; ::GetTokenInformation(hToken, TokenElevationType, &type, sizeof(type), &len))
		props.push_back({ L"Elevation Type", type == TokenElevationTypeFull ? L"Full" : type == TokenElevationTypeLimited ? L"Limited" : L"Default" });
	if (DWORD allowed, enabled; ::GetTokenInformation(hToken, TokenVirtualizationAllowed, &allowed, sizeof(allowed), &len)
		&& ::GetTokenInformation(hToken, TokenVirtualizationEnabled, &enabled, sizeof(enabled), &len))
		props.push_back({ L"Virtualization", !allowed ? L"Not Allowed" : enabled ? L"Enabled" : L"Disabled" });
	if (DWORD uiAccess; ::GetTokenInformation(hToken, TokenUIAccess, &uiAccess, sizeof(uiAccess), &len))
		props.push_back({ L"UI Access", uiAccess ? L"Yes" : L"No" });
	if (DWORD appContainer; ::GetTokenInformation(hToken, TokenIsAppContainer, &appContainer, sizeof(appContainer), &len))
		props.push_back({ L"AppContainer", appContainer ? L"Yes" : L"No" });
	props.push_back({ L"Restricted", ::IsTokenRestricted(hToken) ? L"Yes" : L"No" });

	if (auto groups = GetTokenInfo(hToken, TokenGroups); groups)
		props.push_back({ L"Groups", std::to_wstring(reinterpret_cast<TOKEN_GROUPS*>(groups.get())->GroupCount).c_str() });
	if (auto privs = GetTokenInfo(hToken, TokenPrivileges); privs) {
		auto tp = reinterpret_cast<TOKEN_PRIVILEGES*>(privs.get());
		CString enabled;
		for (DWORD i = 0; i < tp->PrivilegeCount; i++) {
			auto& priv = tp->Privileges[i];
			if ((priv.Attributes & SE_PRIVILEGE_ENABLED) == 0)
				continue;
			WCHAR name[64];
			DWORD size = _countof(name);
			if (::LookupPrivilegeName(nullptr, &priv.Luid, name, &size)) {
				if (!enabled.IsEmpty())
					enabled += L", ";
				enabled += name;
			}
		}
		props.push_back({ L"Privileges", std::to_wstring(tp->PrivilegeCount).c_str() });
		props.push_back({ L"Enabled Privileges", enabled.IsEmpty() ? CString(L"None") : enabled });
	}
	TOKEN_SOURCE source;	// requires TOKEN_QUERY_SOURCE
	if (::GetTokenInformation(hToken, TokenSource, &source, sizeof(source), &len))
		props.push_back({ L"Source", CString(CStringA(source.SourceName, TOKEN_SOURCE_LENGTH)).Trim() });
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetSessionProperties(HANDLE hSession) {
	std::vector<Property> props;
	// named \KernelObjects\Session<n>
	BYTE buffer[512];
	if (!NT_SUCCESS(NT::NtQueryObject(hSession, NT::ObjectNameInformation, buffer, sizeof(buffer), nullptr)))
		return props;
	auto& name = reinterpret_cast<NT::OBJECT_NAME_INFORMATION*>(buffer)->Name;
	CString path(name.Buffer, name.Length / sizeof(WCHAR));
	auto index = path.ReverseFind(L'\\');
	if (index < 0 || path.Mid(index + 1, 7).CompareNoCase(L"Session") != 0 || !iswdigit(path[index + 8]))
		return props;
	auto id = wcstoul(path.Mid(index + 8), nullptr, 10);
	props.push_back({ L"Session ID", std::to_wstring(id).c_str() });

	auto query = [&](WTS_INFO_CLASS infoClass) {
		CString text;
		PWSTR value;
		DWORD bytes;
		if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, id, infoClass, &value, &bytes)) {
			text = value;
			::WTSFreeMemory(value);
		}
		return text;
	};
	PWSTR value;
	DWORD bytes;
	if (::WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE, id, WTSSessionInfo, &value, &bytes)) {
		auto info = reinterpret_cast<WTSINFO*>(value);
		props.push_back({ L"State", ConnectStateToString(info->State) });
		if (info->WinStationName[0])
			props.push_back({ L"Window Station", info->WinStationName });
		if (info->UserName[0])
			props.push_back({ L"User", info->Domain[0] ? CString(info->Domain) + L"\\" + info->UserName : CString(info->UserName) });
		if (info->LogonTime.QuadPart)
			props.push_back({ L"Logon Time", FormatTime(*(FILETIME*)&info->LogonTime) });
		if (info->ConnectTime.QuadPart)
			props.push_back({ L"Connect Time", FormatTime(*(FILETIME*)&info->ConnectTime) });
		if (info->DisconnectTime.QuadPart)
			props.push_back({ L"Disconnect Time", FormatTime(*(FILETIME*)&info->DisconnectTime) });
		::WTSFreeMemory(value);
	}
	if (auto client = query(WTSClientName); !client.IsEmpty())
		props.push_back({ L"Client Name", client });
	return props;
}

std::vector<TypeProperties::Property> TypeProperties::GetFileProperties(HANDLE hFile) {
	std::vector<Property> props;
	BYTE buffer[FileQuery::BufferSize];
	if (!FileQuery::Run(hFile, 0, QueryFileData, buffer)) {
		props.push_back({ L"Note", L"The file couldn't be queried (it may be blocked in a synchronous operation)" });
		return props;
	}

	auto& data = *(FileData*)buffer;
	if (data.HasDevice && !data.HasBasic && !data.HasPipe) {
		//
		// the handle lacks FILE_READ_ATTRIBUTES; open the file again for it (a new file object, so not
		// for the mode and position); opened for asynchronous I/O, the query doesn't block
		//
		if (wil::unique_handle hReopened(ReopenFile(hFile, FILE_READ_ATTRIBUTES)); hReopened) {
			IO_STATUS_BLOCK ioStatus;
			data.HasBasic = NT_SUCCESS(NT::NtQueryInformationFile(hReopened.get(), &ioStatus, &data.Basic, sizeof(data.Basic), NT::FileBasicInformation));
		}
	}
	if (data.HasDevice) {
		auto device = StringHelper::DeviceTypeToString(data.Device.DeviceType);
		props.push_back({ L"Device Type", device ? CString(device) : CString(std::format(L"0x{:X}", data.Device.DeviceType).c_str()) });
		props.push_back({ L"Device Characteristics", std::format(L"0x{:X}", data.Device.Characteristics).c_str() });
	}
	if (data.HasMode) {
		auto sync = data.Mode & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT);
		props.push_back({ L"I/O", !sync ? L"Asynchronous" : (data.Mode & FILE_SYNCHRONOUS_IO_ALERT) ? L"Synchronous (Alertable)" : L"Synchronous" });
	}
	if (data.HasPosition)
		props.push_back({ L"Position", FormatNumber(data.Position.CurrentByteOffset.QuadPart) });
	if (data.HasPipe) {
		auto& pipe = data.Pipe;
		props.push_back({ L"Pipe End", pipe.NamedPipeEnd ? L"Server" : L"Client" });
		if (auto state = StringHelper::PipeStateToString(pipe.NamedPipeState); state)
			props.push_back({ L"Pipe State", state });
		props.push_back({ L"Pipe Type", pipe.NamedPipeType ? L"Message" : L"Byte" });
		props.push_back({ L"Instances", pipe.MaximumInstances == ULONG_MAX ? std::format(L"{} (Unlimited)", pipe.CurrentInstances).c_str()
			: std::format(L"{} of {}", pipe.CurrentInstances, pipe.MaximumInstances).c_str() });
		props.push_back({ L"Read Data Available", FormatSize(pipe.ReadDataAvailable) });
		props.push_back({ L"Inbound Quota", FormatSize(pipe.InboundQuota) });
		props.push_back({ L"Outbound Quota", FormatSize(pipe.OutboundQuota) });
	}
	if (data.HasStandard) {
		props.push_back({ L"Directory", data.Standard.Directory ? L"Yes" : L"No" });
		if (!data.Standard.Directory) {
			props.push_back({ L"Size", FormatSize(data.Standard.EndOfFile.QuadPart) });
			props.push_back({ L"Allocation Size", FormatSize(data.Standard.AllocationSize.QuadPart) });
		}
		props.push_back({ L"Links", std::to_wstring(data.Standard.NumberOfLinks).c_str() });
		props.push_back({ L"Delete Pending", data.Standard.DeletePending ? L"Yes" : L"No" });
	}
	if (data.HasInternal)
		props.push_back({ L"File ID", std::format(L"0x{:016X}", data.Internal.IndexNumber.QuadPart).c_str() });
	if (data.HasBasic) {
		props.push_back({ L"Attributes", std::format(L"0x{:X} ({})", data.Basic.FileAttributes,
			(PCWSTR)StringHelper::FileAttributesToString(data.Basic.FileAttributes)).c_str() });
		props.push_back({ L"Created", FormatFileTime(data.Basic.CreationTime) });
		props.push_back({ L"Modified", FormatFileTime(data.Basic.LastWriteTime) });
		props.push_back({ L"Accessed", FormatFileTime(data.Basic.LastAccessTime) });
		props.push_back({ L"Changed", FormatFileTime(data.Basic.ChangeTime) });
	}
	return props;
}
