#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "TypeProperties.h"
#include "ProcessHelper.h"
#include <thread>
#include <atomic>

namespace {
	CString GetValue(std::vector<TypeProperties::Property> const& props, PCWSTR name) {
		for (auto& prop : props)
			if (prop.Name == name)
				return prop.Value;
		return L"<missing>";
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
