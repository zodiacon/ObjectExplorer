#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TypeProperties.h"
#include "ProcessHelper.h"
#include "NtDll.h"
#include <thread>
#include <atomic>

namespace {
	CString GetValue(std::vector<TypeProperties::Property> const& props, PCWSTR name) {
		for (auto& prop : props)
			if (prop.Name == name)
				return prop.Value;
		return L"<missing>";
	}

	// for INFO
	std::string Dump(std::vector<TypeProperties::Property> const& props) {
		std::string text;
		for (auto& [name, value] : props)
			text += std::string(CW2A(name)) + " = " + std::string(CW2A(value)) + "\n";
		return text;
	}
}

TEST_CASE("Types with a page of their own", "[TypeProperties]") {
	CHECK(TypeProperties::HasProperties(L"Event"));
	CHECK(TypeProperties::HasProperties(L"Mutant"));
	CHECK_FALSE(TypeProperties::HasProperties(L"Directory"));
	CHECK(TypeProperties::GetPageTitle(L"Event") == L"Event");
	CHECK(TypeProperties::GetPageTitle(L"Mutant") == L"Mutex");

	wil::unique_handle hEvent(::CreateEvent(nullptr, TRUE, FALSE, nullptr));
	CHECK(TypeProperties::GetProperties(hEvent.get(), L"Directory").empty());
}

TEST_CASE("Event properties", "[TypeProperties]") {
	SECTION("a signaled manual reset event") {
		wil::unique_handle hEvent(::CreateEvent(nullptr, TRUE, TRUE, nullptr));
		auto props = TypeProperties::GetProperties(hEvent.get(), L"Event");
		CHECK(GetValue(props, L"Event Type") == L"Notification (Manual Reset)");
		CHECK(GetValue(props, L"Signaled") == L"Yes");
	}

	SECTION("a non-signaled auto reset event") {
		wil::unique_handle hEvent(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
		auto props = TypeProperties::GetProperties(hEvent.get(), L"Event");
		CHECK(GetValue(props, L"Event Type") == L"Synchronization (Auto Reset)");
		CHECK(GetValue(props, L"Signaled") == L"No");
	}

	SECTION("a handle without query access, as the views duplicate handles") {
		wil::unique_handle hEvent(::CreateEvent(nullptr, TRUE, TRUE, nullptr));
		HANDLE hDup;
		REQUIRE(::DuplicateHandle(::GetCurrentProcess(), hEvent.get(), ::GetCurrentProcess(), &hDup, READ_CONTROL, FALSE, 0));
		wil::unique_handle dup(hDup);
		auto props = TypeProperties::GetProperties(dup.get(), L"Event");
		CHECK(GetValue(props, L"Event Type") == L"Notification (Manual Reset)");
		CHECK(GetValue(props, L"Signaled") == L"Yes");
	}
}

TEST_CASE("Mutex properties", "[TypeProperties]") {
	wil::unique_handle hMutex(::CreateMutex(nullptr, FALSE, nullptr));
	HANDLE hDup;
	REQUIRE(::DuplicateHandle(::GetCurrentProcess(), hMutex.get(), ::GetCurrentProcess(), &hDup, READ_CONTROL, FALSE, 0));
	wil::unique_handle readControl(hDup);
	CHECK(GetValue(TypeProperties::GetProperties(readControl.get(), L"Mutant"), L"Held") == L"No");

	auto props = TypeProperties::GetProperties(hMutex.get(), L"Mutant");
	CHECK(GetValue(props, L"Held") == L"No");
	CHECK(GetValue(props, L"Signaled") == L"Yes");
	CHECK(GetValue(props, L"Abandoned") == L"No");
	CHECK(GetValue(props, L"Owner Thread") == L"<missing>");

	// acquired twice
	REQUIRE(::WaitForSingleObject(hMutex.get(), 0) == WAIT_OBJECT_0);
	REQUIRE(::WaitForSingleObject(hMutex.get(), 0) == WAIT_OBJECT_0);
	props = TypeProperties::GetProperties(hMutex.get(), L"Mutant");
	CHECK(GetValue(props, L"Held") == L"Yes");
	CHECK(GetValue(props, L"Signaled") == L"No");
	CHECK(GetValue(props, L"Recursion Count") == L"2");
	auto pid = ::GetCurrentProcessId(), tid = ::GetCurrentThreadId();
	CHECK(GetValue(props, L"Owner Process") == std::format(L"{} (0x{:X}) {}", pid, pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str());
	CHECK(GetValue(props, L"Owner Thread") == std::format(L"{} (0x{:X})", tid, tid).c_str());
	::ReleaseMutex(hMutex.get());
	::ReleaseMutex(hMutex.get());
}

namespace {
	bool Contains(CString const& text, PCWSTR part) {
		return text.Find(part) >= 0;
	}

	// the access the views duplicate handles with
	wil::unique_handle ReadControlHandle(HANDLE h) {
		HANDLE hDup = nullptr;
		::DuplicateHandle(::GetCurrentProcess(), h, ::GetCurrentProcess(), &hDup, READ_CONTROL, FALSE, 0);
		return wil::unique_handle(hDup);
	}
}

TEST_CASE("Semaphore properties", "[TypeProperties]") {
	wil::unique_handle hSemaphore(::CreateSemaphore(nullptr, 2, 5, nullptr));
	auto h = ReadControlHandle(hSemaphore.get());
	REQUIRE(h);
	auto props = TypeProperties::GetProperties(h.get(), L"Semaphore");
	CHECK(GetValue(props, L"Signaled") == L"Yes");
	CHECK(GetValue(props, L"Count") == L"2 (0x2)");
	CHECK(GetValue(props, L"Maximum Count") == L"5 (0x5)");

	wil::unique_handle hEmpty(::CreateSemaphore(nullptr, 0, 1, nullptr));
	CHECK(GetValue(TypeProperties::GetProperties(hEmpty.get(), L"Semaphore"), L"Signaled") == L"No");
}

TEST_CASE("Timer properties", "[TypeProperties]") {
	wil::unique_handle hTimer(::CreateWaitableTimer(nullptr, TRUE, nullptr));
	auto h = ReadControlHandle(hTimer.get());
	REQUIRE(h);
	CHECK(GetValue(TypeProperties::GetProperties(h.get(), L"Timer"), L"Signaled") == L"No");

	LARGE_INTEGER due;
	due.QuadPart = -10000000LL * 3600;	// an hour from now
	REQUIRE(::SetWaitableTimer(hTimer.get(), &due, 0, nullptr, nullptr, FALSE));
	auto props = TypeProperties::GetProperties(h.get(), L"Timer");
	CHECK(GetValue(props, L"Signaled") == L"No");
	auto remaining = GetValue(props, L"Remaining Time");
	INFO(CW2A(remaining).m_psz);
	CHECK((Contains(remaining, L"00:59:") || Contains(remaining, L"01:00:00")));

	due.QuadPart = -1;
	REQUIRE(::SetWaitableTimer(hTimer.get(), &due, 0, nullptr, nullptr, FALSE));
	REQUIRE(::WaitForSingleObject(hTimer.get(), 5000) == WAIT_OBJECT_0);
	props = TypeProperties::GetProperties(h.get(), L"Timer");
	CHECK(GetValue(props, L"Signaled") == L"Yes");
	CHECK(GetValue(props, L"Remaining Time") == L"<missing>");
}

TEST_CASE("Section properties", "[TypeProperties]") {
	SECTION("a page file backed section") {
		wil::unique_handle hSection(::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1 << 16, nullptr));
		auto h = ReadControlHandle(hSection.get());
		REQUIRE(h);
		auto props = TypeProperties::GetProperties(h.get(), L"Section");
		CHECK(GetValue(props, L"Size") == L"65536 bytes (0x10000)");
		CHECK(Contains(GetValue(props, L"Attributes"), L"Commit"));
		CHECK(GetValue(props, L"Machine") == L"<missing>");
	}

	SECTION("an image section") {
		WCHAR path[MAX_PATH];
		::GetModuleFileName(nullptr, path, _countof(path));
		wil::unique_hfile file(::CreateFile(path, GENERIC_READ | GENERIC_EXECUTE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(file);
		wil::unique_handle hSection(::CreateFileMapping(file.get(), nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr));
		REQUIRE(hSection);
		auto props = TypeProperties::GetProperties(hSection.get(), L"Section");
		CHECK(Contains(GetValue(props, L"Attributes"), L"Image"));
		CHECK(GetValue(props, L"Machine") == L"x64");
		CHECK(Contains(GetValue(props, L"Subsystem"), L"Windows Console"));
		CHECK(GetValue(props, L"Contains Code") == L"Yes");
		CHECK(GetValue(props, L"Entry Point") != L"<missing>");
	}
}

TEST_CASE("Process properties", "[TypeProperties]") {
	SECTION("the current process") {
		auto pid = ::GetCurrentProcessId();
		wil::unique_handle hProcess(::OpenProcess(READ_CONTROL, FALSE, pid));
		REQUIRE(hProcess);
		auto props = TypeProperties::GetProperties(hProcess.get(), L"Process");
		CHECK(GetValue(props, L"Process ID") == std::format(L"{} (0x{:X})", pid, pid).c_str());
		CHECK(GetValue(props, L"Image Name") == ProcessHelper::GetProcessName(pid));
		CHECK(Contains(GetValue(props, L"Image Path"), L"ObjExpTests.exe"));
		CHECK(Contains(GetValue(props, L"Command Line"), L"ObjExpTests"));
		CHECK(GetValue(props, L"User") != L"<missing>");
		CHECK(GetValue(props, L"Integrity Level") != L"<missing>");
		CHECK(GetValue(props, L"Architecture") != L"<missing>");
		CHECK(GetValue(props, L"Priority Class") == L"Normal");
		CHECK(GetValue(props, L"Started") != L"<missing>");
		CHECK(GetValue(props, L"Exit Code") == L"<missing>");
	}

	SECTION("an exited process") {
		STARTUPINFO si{ sizeof(si) };
		PROCESS_INFORMATION pi;
		WCHAR cmd[] = L"cmd.exe /c exit 3";
		REQUIRE(::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi));
		wil::unique_handle hProcess(pi.hProcess), hThread(pi.hThread);
		REQUIRE(::WaitForSingleObject(hProcess.get(), 10000) == WAIT_OBJECT_0);
		auto h = ReadControlHandle(hProcess.get());
		REQUIRE(h);
		auto props = TypeProperties::GetProperties(h.get(), L"Process");
		CHECK(GetValue(props, L"Exit Code") == L"0x3");
		CHECK(GetValue(props, L"Exited") != L"<missing>");
		CHECK(GetValue(props, L"Handles") == L"<missing>");
	}
}

TEST_CASE("Thread properties", "[TypeProperties]") {
	std::atomic<bool> stop{ false };
	std::thread worker([&] {
		::SetThreadDescription(::GetCurrentThread(), L"TypeProperties test");
		::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		while (!stop)
			::Sleep(10);
	});
	auto native = (HANDLE)worker.native_handle();
	auto tid = ::GetThreadId(native);
	::Sleep(100);	// let it set its description and priority

	wil::unique_handle hThread(::OpenThread(READ_CONTROL, FALSE, tid));
	REQUIRE(hThread);
	auto props = TypeProperties::GetProperties(hThread.get(), L"Thread");
	stop = true;
	worker.join();

	auto pid = ::GetCurrentProcessId();
	CHECK(GetValue(props, L"Thread ID") == std::format(L"{} (0x{:X})", tid, tid).c_str());
	CHECK(GetValue(props, L"Process") == std::format(L"{} (0x{:X}) {}", pid, pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str());
	CHECK(GetValue(props, L"Description") == L"TypeProperties test");
	CHECK(GetValue(props, L"Relative Priority") == L"Above Normal");
	CHECK(GetValue(props, L"TEB Address") != L"<missing>");
	CHECK(GetValue(props, L"CPU Time") != L"<missing>");
	CHECK(GetValue(props, L"Exit Code") == L"<missing>");
}

TEST_CASE("Job properties", "[TypeProperties]") {
	wil::unique_handle hJob(::CreateJobObject(nullptr, nullptr));
	REQUIRE(hJob);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_ACTIVE_PROCESS | JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	limits.BasicLimitInformation.ActiveProcessLimit = 5;
	REQUIRE(::SetInformationJobObject(hJob.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)));

	STARTUPINFO si{ sizeof(si) };
	PROCESS_INFORMATION pi;
	WCHAR cmd[] = L"cmd.exe /c exit";
	REQUIRE(::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi));
	wil::unique_handle hProcess(pi.hProcess), hThread(pi.hThread);
	REQUIRE(::AssignProcessToJobObject(hJob.get(), hProcess.get()));

	auto h = ReadControlHandle(hJob.get());
	REQUIRE(h);
	auto props = TypeProperties::GetProperties(h.get(), L"Job");
	::TerminateJobObject(hJob.get(), 0);

	CHECK(GetValue(props, L"Active Processes") == L"1");
	CHECK(GetValue(props, L"Total Processes") == L"1");
	CHECK(Contains(GetValue(props, L"Processes"), std::format(L"{} (", pi.dwProcessId).c_str()));
	CHECK(GetValue(props, L"Limits") == L"Active Processes, Kill on Job Close");
	CHECK(GetValue(props, L"Active Process Limit") == L"5");
	CHECK(GetValue(props, L"Job Memory Limit") == L"<missing>");
}

TEST_CASE("Window station properties", "[TypeProperties]") {
	auto hWinSta = ::GetProcessWindowStation();	// not to be closed
	REQUIRE(hWinSta);
	WCHAR name[256];
	DWORD len;
	REQUIRE(::GetUserObjectInformation(hWinSta, UOI_NAME, name, sizeof(name), &len));
	auto h = ReadControlHandle(hWinSta);
	REQUIRE(h);

	SECTION("in this session") {
		auto props = TypeProperties::GetProperties(h.get(), L"WindowStation");
		CHECK(GetValue(props, L"Name") == name);
		CHECK(GetValue(props, L"Interactive") != L"<missing>");
		CHECK(GetValue(props, L"Desktops") != L"<missing>");
		CHECK(GetValue(props, L"Note") == L"<missing>");
	}

	SECTION("duplicated from a process (here: this one)") {
		auto props = TypeProperties::GetProperties(h.get(), L"WindowStation", ::GetCurrentProcessId());
		CHECK(GetValue(props, L"Name") == name);
	}
}

TEST_CASE("Desktop properties", "[TypeProperties]") {
	auto hDesktop = ::GetThreadDesktop(::GetCurrentThreadId());	// not to be closed
	REQUIRE(hDesktop);
	WCHAR name[256];
	DWORD len;
	REQUIRE(::GetUserObjectInformation(hDesktop, UOI_NAME, name, sizeof(name), &len));
	auto h = ReadControlHandle(hDesktop);
	REQUIRE(h);

	SECTION("in this session") {
		auto props = TypeProperties::GetProperties(h.get(), L"Desktop");
		CHECK(GetValue(props, L"Name") == name);
		CHECK(GetValue(props, L"Heap Size") != L"<missing>");
		CHECK(GetValue(props, L"Receives Input") != L"<missing>");
	}

	SECTION("from a process in another session isn't queried") {
		// the System process is in session 0, and the tests don't run there
		DWORD session;
		REQUIRE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session));
		if (session == 0)
			SKIP("running in session 0");
		auto props = TypeProperties::GetProperties(h.get(), L"Desktop", 4);
		CHECK(GetValue(props, L"Name") == L"<missing>");
		// the System process can't be opened without elevation, so the session may be unknown
		CHECK((GetValue(props, L"Session") == L"0" || GetValue(props, L"Session") == L"Unknown"));
		CHECK(GetValue(props, L"Note") != L"<missing>");
	}
}

TEST_CASE("Key properties", "[TypeProperties]") {
	auto path = std::format(L"Software\\ObjExpTests-{}", ::GetCurrentProcessId());
	wil::unique_hkey hKey;
	REQUIRE(::RegCreateKeyEx(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, hKey.addressof(), nullptr) == ERROR_SUCCESS);
	wil::unique_hkey hSubKey;
	REQUIRE(::RegCreateKeyEx(hKey.get(), L"Sub", 0, nullptr, REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, hSubKey.addressof(), nullptr) == ERROR_SUCCESS);
	DWORD value = 42;
	REQUIRE(::RegSetValueEx(hKey.get(), L"Answer", 0, REG_DWORD, (BYTE*)&value, sizeof(value)) == ERROR_SUCCESS);

	auto h = ReadControlHandle(hKey.get());
	REQUIRE(h);
	auto props = TypeProperties::GetProperties(h.get(), L"Key");
	hSubKey.reset();
	::RegDeleteTree(hKey.get(), nullptr);
	hKey.reset();
	::RegDeleteKey(HKEY_CURRENT_USER, path.c_str());
	INFO(Dump(props));

	CHECK(GetValue(props, L"Subkeys") == L"1");
	CHECK(GetValue(props, L"Values") == L"1");
	CHECK(GetValue(props, L"Longest Subkey Name") == L"3 characters");
	CHECK(GetValue(props, L"Longest Value Name") == L"6 characters");
	CHECK(GetValue(props, L"Largest Value Data") == L"4 bytes (0x4)");
	CHECK(GetValue(props, L"Last Write") != L"<missing>");
	CHECK(GetValue(props, L"Volatile") == L"Yes");
	CHECK(GetValue(props, L"Symbolic Link") == L"No");
	CHECK(GetValue(props, L"Virtualization") != L"<missing>");
}

TEST_CASE("ALPC port properties", "[TypeProperties]") {
	NT::ALPC_PORT_ATTRIBUTES attributes{};
	attributes.Flags = 0x10000 | 0x40000;	// allow impersonation, waitable
	attributes.MaxMessageLength = 0x1000;
	attributes.SecurityQos.Length = sizeof(attributes.SecurityQos);
	attributes.SecurityQos.ImpersonationLevel = SecurityImpersonation;
	attributes.SecurityQos.ContextTrackingMode = SECURITY_DYNAMIC_TRACKING;
	HANDLE hPort;
	REQUIRE(NT_SUCCESS(NT::NtAlpcCreatePort(&hPort, nullptr, &attributes)));
	wil::unique_handle port(hPort);

	auto h = ReadControlHandle(port.get());
	REQUIRE(h);
	auto props = TypeProperties::GetProperties(h.get(), L"ALPC Port");
	auto flags = GetValue(props, L"Flags");
	INFO(Dump(props));
	CHECK(Contains(flags, L"Allow Impersonation"));
	CHECK(Contains(flags, L"Waitable"));
	CHECK(GetValue(props, L"Sequence Number") != L"<missing>");
	// a connection port's server is the process that created it
	auto pid = ::GetCurrentProcessId();
	CHECK(GetValue(props, L"Server Process") == std::format(L"{} (0x{:X}) {}", pid, pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str());
}

TEST_CASE("Token properties", "[TypeProperties]") {
	wil::unique_handle hToken;
	REQUIRE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, hToken.addressof()));
	auto h = ReadControlHandle(hToken.get());
	REQUIRE(h);
	auto props = TypeProperties::GetProperties(h.get(), L"Token");
	INFO(Dump(props));

	WCHAR user[256], domain[256];
	DWORD userSize = _countof(user), domainSize = _countof(domain);
	BYTE buffer[256];
	DWORD len;
	REQUIRE(::GetTokenInformation(hToken.get(), TokenUser, buffer, sizeof(buffer), &len));
	SID_NAME_USE use;
	REQUIRE(::LookupAccountSid(nullptr, reinterpret_cast<TOKEN_USER*>(buffer)->User.Sid, user, &userSize, domain, &domainSize, &use));
	CHECK(GetValue(props, L"User") == CString(domain) + L"\\" + user);

	DWORD session;
	REQUIRE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session));
	CHECK(GetValue(props, L"Session") == std::to_wstring(session).c_str());
	CHECK(GetValue(props, L"Type") == L"Primary");
	CHECK(GetValue(props, L"Integrity Level") != L"<missing>");
	CHECK(GetValue(props, L"Logon Session") != L"<missing>");
	CHECK(GetValue(props, L"AppContainer") == L"No");
	CHECK(Contains(GetValue(props, L"Enabled Privileges"), L"SeChangeNotifyPrivilege"));

	SECTION("an impersonation token") {
		wil::unique_handle hImp;
		REQUIRE(::DuplicateTokenEx(hToken.get(), TOKEN_QUERY, nullptr, SecurityIdentification, TokenImpersonation, hImp.addressof()));
		auto hi = ReadControlHandle(hImp.get());
		REQUIRE(hi);
		CHECK(GetValue(TypeProperties::GetProperties(hi.get(), L"Token"), L"Type") == L"Impersonation (Identification)");
	}
}

TEST_CASE("Session properties", "[TypeProperties]") {
	DWORD session;
	REQUIRE(::ProcessIdToSessionId(::GetCurrentProcessId(), &session));
	auto name = std::format(L"\\KernelObjects\\Session{}", session);
	UNICODE_STRING uname;
	::RtlInitUnicodeString(&uname, name.c_str());
	OBJECT_ATTRIBUTES attr;
	InitializeObjectAttributes(&attr, &uname, 0, nullptr, nullptr);
	HANDLE hSession;
	NTSTATUS status = STATUS_ACCESS_DENIED;
	for (ACCESS_MASK access : { (ACCESS_MASK)READ_CONTROL, (ACCESS_MASK)0x1 /* SESSION_QUERY_ACCESS */, (ACCESS_MASK)0 })
		if (status = NT::NtOpenSession(&hSession, access, &attr); NT_SUCCESS(status))
			break;
	if (!NT_SUCCESS(status))
		SKIP("Can't open the session object: 0x" << std::hex << status);
	wil::unique_handle h(hSession);

	auto props = TypeProperties::GetProperties(h.get(), L"Session");
	INFO(Dump(props));
	CHECK(GetValue(props, L"Session ID") == std::to_wstring(session).c_str());
	CHECK(GetValue(props, L"State") != L"<missing>");

	WCHAR user[256];
	DWORD size = _countof(user);
	REQUIRE(::GetUserName(user, &size));
	CHECK(Contains(GetValue(props, L"User"), user));
}

TEST_CASE("File properties", "[TypeProperties]") {
	SECTION("a disk file") {
		WCHAR path[MAX_PATH];
		::GetModuleFileName(nullptr, path, _countof(path));
		wil::unique_hfile file(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(file);
		LARGE_INTEGER size;
		REQUIRE(::GetFileSizeEx(file.get(), &size));
		LARGE_INTEGER offset{ .QuadPart = 100 };
		REQUIRE(::SetFilePointerEx(file.get(), offset, nullptr, FILE_BEGIN));

		auto h = ReadControlHandle(file.get());
		REQUIRE(h);
		auto props = TypeProperties::GetProperties(h.get(), L"File");
		INFO(Dump(props));
		CHECK(GetValue(props, L"Device Type") == L"Disk");
		CHECK(GetValue(props, L"I/O") == L"Synchronous");
		CHECK(GetValue(props, L"Position") == L"100 (0x64)");
		CHECK(GetValue(props, L"Directory") == L"No");
		CHECK(GetValue(props, L"Size") == std::format(L"{} bytes (0x{:X})", size.QuadPart, size.QuadPart).c_str());
		CHECK(GetValue(props, L"Delete Pending") == L"No");
		CHECK(GetValue(props, L"File ID") != L"<missing>");
		// requires FILE_READ_ATTRIBUTES, which the file is opened again for
		CHECK(GetValue(props, L"Modified") != L"<missing>");
		CHECK(GetValue(props, L"Attributes") != L"<missing>");
	}

	SECTION("a named pipe's server end") {
		auto name = std::format(L"\\\\.\\pipe\\ObjExpTests-{}", ::GetCurrentProcessId());
		wil::unique_hfile server(::CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			3, 4096, 4096, 0, nullptr));
		REQUIRE(server);
		wil::unique_hfile client(::CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(client);

		auto h = ReadControlHandle(server.get());
		REQUIRE(h);
		auto props = TypeProperties::GetProperties(h.get(), L"File");
		INFO(Dump(props));
		CHECK(GetValue(props, L"Device Type") == L"Named Pipe");
		CHECK(GetValue(props, L"I/O") == L"Asynchronous");
		// requires FILE_READ_ATTRIBUTES; pipes aren't opened again, so only with the handle's own access
		props = TypeProperties::GetProperties(server.get(), L"File");
		INFO(Dump(props));
		CHECK(GetValue(props, L"Pipe End") == L"Server");
		CHECK(GetValue(props, L"Pipe State") == L"Connected");
		CHECK(GetValue(props, L"Pipe Type") == L"Message");
		CHECK(GetValue(props, L"Instances") == L"1 of 3");
	}
}
