#include "pch.h"
#include "ObjectDetails.h"
#include "ObjectManager.h"
#include "ProcessHelper.h"
#include "StringHelper.h"
#include "FileQuery.h"
#include <TlHelp32.h>

namespace {
	CString FormatTime(FILETIME const& ft) {
		return CTime(ft).Format(L"%c");
	}

	void Append(CString& text, PCWSTR label, CString const& value) {
		if (value.IsEmpty())
			return;
		if (!text.IsEmpty())
			text += L", ";
		text += label;
		text += L": ";
		text += value;
	}

	PCWSTR DeviceTypeToString(ULONG type) {
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

	CString FileAttributesToString(ULONG attributes) {
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

	PCWSTR PipeStateToString(ULONG state) {
		switch (state) {
			case 1: return L"Disconnected";
			case 2: return L"Listening";
			case 3: return L"Connected";
			case 4: return L"Closing";
		}
		return nullptr;
	}

	//
	// collected on a FileQuery worker thread
	//
	struct FileData {
		NT::FILE_FS_DEVICE_INFORMATION Device;
		NT::FILE_STANDARD_INFORMATION Standard;
		NT::FILE_BASIC_INFORMATION Basic;
		NT::FILE_PIPE_LOCAL_INFORMATION Pipe;
		ULONG Mode;
		bool HasDevice, HasStandard, HasBasic, HasPipe, HasMode;
	};
	static_assert(sizeof(FileData) <= FileQuery::BufferSize);

	const ULONG NamedPipeDevice = 0x11;

	bool QueryFile(HANDLE hFile, BYTE* buffer) {
		auto data = new (buffer) FileData{};
		IO_STATUS_BLOCK ioStatus;
		data->HasDevice = NT_SUCCESS(NT::NtQueryVolumeInformationFile(hFile, &ioStatus, &data->Device, sizeof(data->Device), NT::FileFsDeviceInformation));
		NT::FILE_MODE_INFORMATION mode;
		data->HasMode = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &mode, sizeof(mode), NT::FileModeInformation));
		data->Mode = mode.Mode;
		if (data->HasDevice && data->Device.DeviceType == NamedPipeDevice)
			data->HasPipe = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Pipe, sizeof(data->Pipe), NT::FilePipeLocalInformation));
		else {
			data->HasStandard = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Standard, sizeof(data->Standard), NT::FileStandardInformation));
			data->HasBasic = NT_SUCCESS(NT::NtQueryInformationFile(hFile, &ioStatus, &data->Basic, sizeof(data->Basic), NT::FileBasicInformation));
		}
		return data->HasDevice || data->HasMode || data->HasStandard || data->HasBasic || data->HasPipe;
	}
}

CString ObjectDetails::GetDetails(HANDLE hObject, DWORD pid, PCWSTR type, PVOID object) {
	if (::_wcsicmp(type, L"WindowStation") == 0 || ::_wcsicmp(type, L"Desktop") == 0) {
		//
		// these live in session space; only query those of this session
		//
		DWORD session, ourSession;
		if (!::ProcessIdToSessionId(pid, &session) || !::ProcessIdToSessionId(::GetCurrentProcessId(), &ourSession) || session != ourSession)
			return L"";
	}
	wil::unique_handle hDup(ObjectManager::DupHandle(hObject, pid, 0, DUPLICATE_SAME_ACCESS));
	if (!hDup)
		return L"";
	// the same key the file name query uses, so an object whose queries get stuck isn't queried again
	auto key = object ? (ULONG64)(ULONG_PTR)object : ((ULONG64)pid << 32) | HandleToULong(hObject);
	return GetDetails(hDup.get(), type, key);
}

CString ObjectDetails::GetDetails(HANDLE hObject, PCWSTR type, ULONG64 key) {
	CString text;
	if (::_wcsicmp(type, L"File") == 0)
		return GetFileDetails(hObject, key);
	if (::_wcsicmp(type, L"WindowStation") == 0)
		return GetWindowStationDetails(hObject);
	if (::_wcsicmp(type, L"Desktop") == 0)
		return GetDesktopDetails(hObject);
	if (::_wcsicmp(type, L"Process") == 0)
		return GetProcessDetails(hObject);
	if (::_wcsicmp(type, L"Thread") == 0)
		return GetThreadDetails(hObject);
	if (::_wcsicmp(type, L"Token") == 0)
		return GetTokenDetails(hObject);
	if (::_wcsicmp(type, L"Key") == 0)
		return GetKeyDetails(hObject);

	if (::_wcsicmp(type, L"Event") == 0) {
		NT::EVENT_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryEvent(hObject, NT::EventBasicInformation, &info, sizeof(info), nullptr))) {
			Append(text, L"Type", info.EventType == NT::NotificationEvent ? L"Manual Reset" : L"Auto Reset");
			Append(text, L"Signaled", info.EventState ? L"Yes" : L"No");
		}
	}
	else if (::_wcsicmp(type, L"Mutant") == 0) {
		NT::MUTANT_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryMutant(hObject, NT::MutantBasicInformation, &info, sizeof(info), nullptr))) {
			Append(text, L"Held", info.CurrentCount <= 0 ? L"Yes" : L"No");
			if (info.AbandonedState)
				Append(text, L"Abandoned", L"Yes");
		}
		NT::MUTANT_OWNER_INFORMATION owner;
		if (NT_SUCCESS(NT::NtQueryMutant(hObject, NT::MutantOwnerInformation, &owner, sizeof(owner), nullptr)) && owner.ClientId.UniqueThread) {
			auto ownerPid = HandleToUlong(owner.ClientId.UniqueProcess);
			Append(text, L"Owner", std::format(L"{} ({}) TID {}", ownerPid, (PCWSTR)ProcessHelper::GetProcessName(ownerPid),
				HandleToUlong(owner.ClientId.UniqueThread)).c_str());
		}
	}
	else if (::_wcsicmp(type, L"Semaphore") == 0) {
		NT::SEMAPHORE_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQuerySemaphore(hObject, NT::SemaphoreBasicInformation, &info, sizeof(info), nullptr))) {
			Append(text, L"Count", std::to_wstring(info.CurrentCount).c_str());
			Append(text, L"Maximum", std::to_wstring(info.MaximumCount).c_str());
		}
	}
	else if (::_wcsicmp(type, L"Timer") == 0) {
		NT::TIMER_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryTimer(hObject, NT::TimerBasicInformation, &info, sizeof(info), nullptr))) {
			Append(text, L"Signaled", info.TimerState ? L"Yes" : L"No");
			if (!info.TimerState && info.RemainingTime.QuadPart > 0)
				Append(text, L"Remaining", std::format(L"{} msec", info.RemainingTime.QuadPart / 10000).c_str());
		}
	}
	else if (::_wcsicmp(type, L"IoCompletion") == 0) {
		NT::IO_COMPLETION_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQueryIoCompletion(hObject, NT::IoCompletionBasicInformation, &info, sizeof(info), nullptr)))
			Append(text, L"Queued Packets", std::to_wstring(info.Depth).c_str());
	}
	else if (::_wcsicmp(type, L"Section") == 0) {
		NT::SECTION_BASIC_INFORMATION info;
		if (NT_SUCCESS(NT::NtQuerySection(hObject, NT::SectionBasicInformation, &info, sizeof(info), nullptr))) {
			Append(text, L"Size", std::format(L"0x{:X}", info.MaximumSize.QuadPart).c_str());
			Append(text, L"Attributes", StringHelper::SectionAttributesToString(info.AllocationAttributes));
		}
	}
	else if (::_wcsicmp(type, L"Job") == 0) {
		JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info;
		if (::QueryInformationJobObject(hObject, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr)) {
			Append(text, L"Active Processes", std::to_wstring(info.ActiveProcesses).c_str());
			Append(text, L"Total Processes", std::to_wstring(info.TotalProcesses).c_str());
		}
	}
	else if (::_wcsicmp(type, L"SymbolicLink") == 0) {
		WCHAR buffer[1024];
		UNICODE_STRING target{ 0, (USHORT)sizeof(buffer), buffer };
		if (NT_SUCCESS(NT::NtQuerySymbolicLinkObject(hObject, &target, nullptr)))
			Append(text, L"Target", CString(target.Buffer, target.Length / sizeof(WCHAR)));
	}
	return text;
}

CString ObjectDetails::GetProcessDetails(HANDLE hProcess) {
	CString text;
	auto pid = ::GetProcessId(hProcess);
	if (pid == 0)
		return text;

	Append(text, L"PID", std::format(L"{} ({})", pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str());
	DWORD code;
	bool exited = ::GetExitCodeProcess(hProcess, &code) && code != STILL_ACTIVE;
	if (exited)
		Append(text, L"Exit Code", std::format(L"0x{:X}", code).c_str());
	else {
		if (auto threads = GetThreadCount(pid); threads)
			Append(text, L"Threads", std::to_wstring(threads).c_str());
		if (DWORD handles; ::GetProcessHandleCount(hProcess, &handles))
			Append(text, L"Handles", std::to_wstring(handles).c_str());
	}
	if (DWORD session; ::ProcessIdToSessionId(pid, &session))
		Append(text, L"Session", std::to_wstring(session).c_str());
	if (!exited)
		Append(text, L"User", ProcessHelper::GetUserName(pid).c_str());
	if (FILETIME create, exit, kernel, user; ::GetProcessTimes(hProcess, &create, &exit, &kernel, &user) && create.dwHighDateTime) {
		Append(text, L"Started", FormatTime(create));
		if (exited)
			Append(text, L"Exited", FormatTime(exit));
	}
	return text;
}

CString ObjectDetails::GetThreadDetails(HANDLE hThread) {
	CString text;
	NT::THREAD_BASIC_INFORMATION info;
	if (!NT_SUCCESS(NT::NtQueryInformationThread(hThread, NT::ThreadBasicInformation, &info, sizeof(info), nullptr)))
		return text;

	auto pid = HandleToUlong(info.ClientId.UniqueProcess);
	Append(text, L"TID", std::to_wstring(HandleToUlong(info.ClientId.UniqueThread)).c_str());
	Append(text, L"PID", std::format(L"{} ({})", pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str());
	bool exited = (DWORD)info.ExitStatus != STILL_ACTIVE;
	if (exited)
		Append(text, L"Exit Code", std::format(L"0x{:X}", (DWORD)info.ExitStatus).c_str());
	else
		Append(text, L"Priority", std::to_wstring(info.Priority).c_str());
	if (FILETIME create, exit, kernel, user; ::GetThreadTimes(hThread, &create, &exit, &kernel, &user) && create.dwHighDateTime) {
		Append(text, L"Started", FormatTime(create));
		if (exited)
			Append(text, L"Exited", FormatTime(exit));
	}
	return text;
}

CString ObjectDetails::GetTokenDetails(HANDLE hToken) {
	CString text;
	BYTE buffer[256];
	DWORD len;
	if (::GetTokenInformation(hToken, TokenUser, buffer, sizeof(buffer), &len))
		Append(text, L"User", StringHelper::SidToName(reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid));
	if (TOKEN_TYPE type; ::GetTokenInformation(hToken, TokenType, &type, sizeof(type), &len))
		Append(text, L"Type", type == TokenPrimary ? L"Primary" : L"Impersonation");
	if (DWORD session; ::GetTokenInformation(hToken, TokenSessionId, &session, sizeof(session), &len))
		Append(text, L"Session", std::to_wstring(session).c_str());
	if (::GetTokenInformation(hToken, TokenIntegrityLevel, buffer, sizeof(buffer), &len)) {
		auto sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(buffer)->Label.Sid;
		auto count = *::GetSidSubAuthorityCount(sid);
		if (count)
			Append(text, L"Integrity", StringHelper::IntegrityLevelToString(*::GetSidSubAuthority(sid, count - 1)));
	}
	if (TOKEN_ELEVATION elevation; ::GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &len))
		Append(text, L"Elevated", elevation.TokenIsElevated ? L"Yes" : L"No");
	return text;
}

CString ObjectDetails::GetKeyDetails(HANDLE hKey) {
	CString text;
	DWORD subkeys, values;
	FILETIME lastWrite;
	if (::RegQueryInfoKey((HKEY)hKey, nullptr, nullptr, nullptr, &subkeys, nullptr, nullptr, &values,
		nullptr, nullptr, nullptr, &lastWrite) == ERROR_SUCCESS) {
		Append(text, L"Subkeys", std::to_wstring(subkeys).c_str());
		Append(text, L"Values", std::to_wstring(values).c_str());
		Append(text, L"Last Write", FormatTime(lastWrite));
	}
	return text;
}

CString ObjectDetails::GetFileDetails(HANDLE hFile, ULONG64 key) {
	CString text;
	BYTE buffer[FileQuery::BufferSize];
	if (!FileQuery::Run(hFile, key, QueryFile, buffer))
		return text;

	auto& data = *(FileData*)buffer;
	if (data.HasDevice) {
		auto device = DeviceTypeToString(data.Device.DeviceType);
		Append(text, L"Device", device ? CString(device) : CString(std::format(L"0x{:X}", data.Device.DeviceType).c_str()));
	}
	if (data.HasPipe) {
		auto& pipe = data.Pipe;
		Append(text, L"End", pipe.NamedPipeEnd ? L"Server" : L"Client");
		Append(text, L"State", PipeStateToString(pipe.NamedPipeState));
		Append(text, L"Instances", pipe.MaximumInstances == ULONG_MAX ? std::format(L"{} (Unlimited)", pipe.CurrentInstances).c_str()
			: std::format(L"{} of {}", pipe.CurrentInstances, pipe.MaximumInstances).c_str());
		Append(text, L"Available", std::format(L"{} bytes", pipe.ReadDataAvailable).c_str());
	}
	if (data.HasStandard) {
		if (data.Standard.Directory)
			Append(text, L"Directory", L"Yes");
		else
			Append(text, L"Size", std::format(L"{} bytes", data.Standard.EndOfFile.QuadPart).c_str());
		if (data.Standard.DeletePending)
			Append(text, L"Delete Pending", L"Yes");
	}
	if (data.HasBasic) {
		Append(text, L"Attributes", FileAttributesToString(data.Basic.FileAttributes));
		if (data.Basic.LastWriteTime.QuadPart)
			Append(text, L"Modified", FormatTime(*(FILETIME*)&data.Basic.LastWriteTime));
	}
	if (data.HasMode)
		Append(text, L"I/O", (data.Mode & (FILE_SYNCHRONOUS_IO_ALERT | FILE_SYNCHRONOUS_IO_NONALERT)) ? L"Synchronous" : L"Asynchronous");
	return text;
}

CString ObjectDetails::GetWindowStationDetails(HANDLE hWinSta) {
	CString text;
	DWORD len;
	if (USEROBJECTFLAGS flags; ::GetUserObjectInformation(hWinSta, UOI_FLAGS, &flags, sizeof(flags), &len))
		Append(text, L"Interactive", (flags.dwFlags & WSF_VISIBLE) ? L"Yes" : L"No");
	if (BYTE sid[SECURITY_MAX_SID_SIZE]; ::GetUserObjectInformation(hWinSta, UOI_USER_SID, sid, sizeof(sid), &len) && len)
		Append(text, L"User", StringHelper::SidToName((PSID)sid));

	CString desktops;
	::EnumDesktops((HWINSTA)hWinSta, [](auto name, auto param) {
		auto& desktops = *(CString*)param;
		if (!desktops.IsEmpty())
			desktops += L"; ";
		desktops += name;
		return TRUE;
		}, (LPARAM)&desktops);
	Append(text, L"Desktops", desktops);
	return text;
}

CString ObjectDetails::GetDesktopDetails(HANDLE hDesktop) {
	CString text;
	DWORD len;
	if (BOOL input; ::GetUserObjectInformation(hDesktop, UOI_IO, &input, sizeof(input), &len))
		Append(text, L"Receives Input", input ? L"Yes" : L"No");
	if (ULONG heap; ::GetUserObjectInformation(hDesktop, UOI_HEAPSIZE, &heap, sizeof(heap), &len))
		Append(text, L"Heap", std::format(L"{} KB", heap).c_str());
	if (USEROBJECTFLAGS flags; ::GetUserObjectInformation(hDesktop, UOI_FLAGS, &flags, sizeof(flags), &len) && (flags.dwFlags & DF_ALLOWOTHERACCOUNTHOOK))
		Append(text, L"Hooks", L"Other Accounts Allowed");
	return text;
}

DWORD ObjectDetails::GetThreadCount(DWORD pid) {
	// processes and threads come and go; a newer snapshot isn't needed for every row
	const DWORD64 MaxAge = 2000;
	auto now = ::GetTickCount64();
	{
		auto lock = s_lock.lock_shared();
		if (now - s_lastSnapshot < MaxAge) {
			auto it = s_threadCounts.find(pid);
			return it == s_threadCounts.end() ? 0 : it->second;
		}
	}

	std::unordered_map<DWORD, DWORD> counts;
	wil::unique_handle hSnapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (hSnapshot) {
		PROCESSENTRY32 pe;
		pe.dwSize = sizeof(pe);
		if (::Process32First(hSnapshot.get(), &pe)) {
			counts.reserve(512);
			do {
				counts.insert({ pe.th32ProcessID, pe.cntThreads });
			} while (::Process32Next(hSnapshot.get(), &pe));
		}
	}

	auto lock = s_lock.lock_exclusive();
	s_threadCounts = std::move(counts);
	s_lastSnapshot = ::GetTickCount64();
	auto it = s_threadCounts.find(pid);
	return it == s_threadCounts.end() ? 0 : it->second;
}
