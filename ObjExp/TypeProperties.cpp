#include "pch.h"
#include "TypeProperties.h"
#include "NtDll.h"
#include "ProcessHelper.h"
#include "DriverHelper.h"
#include "StringHelper.h"
#include <atltime.h>

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

std::vector<TypeProperties::Property> TypeProperties::GetProperties(HANDLE hObject, PCWSTR type) {
	auto entry = FindType(type);
	if (!entry)
		return {};
	auto hQuery = DuplicateForQuery(hObject, entry->QueryAccess);
	return entry->GetProperties(hQuery ? hQuery.get() : hObject);
}

wil::unique_handle TypeProperties::DuplicateForQuery(HANDLE hObject, ACCESS_MASK access) {
	// access beyond the handle's is checked against the object's security descriptor
	HANDLE hDup;
	if (::DuplicateHandle(::GetCurrentProcess(), hObject, ::GetCurrentProcess(), &hDup, access, FALSE, 0))
		return wil::unique_handle(hDup);
	// the driver duplicates in kernel mode, without the access check (requires elevation)
	return wil::unique_handle(DriverHelper::DupHandle(hObject, ::GetCurrentProcessId(), access, 0));
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
