#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "ObjectDetails.h"
#include "ProcessHelper.h"
#include <thread>

namespace {
	bool Contains(CString const& text, PCWSTR part) {
		return text.Find(part) >= 0;
	}
}

TEST_CASE("Details of the current process", "[ObjectDetails]") {
	auto pid = ::GetCurrentProcessId();
	auto text = ObjectDetails::GetDetails(::GetCurrentProcess(), L"Process");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, std::format(L"PID: {} ({})", pid, (PCWSTR)ProcessHelper::GetProcessName(pid)).c_str()));
	CHECK(Contains(text, L"Threads: "));
	CHECK(Contains(text, L"Handles: "));
	CHECK(Contains(text, L"User: "));
	CHECK(Contains(text, L"Started: "));
	CHECK_FALSE(Contains(text, L"Exit Code"));
}

TEST_CASE("Details of an exited process", "[ObjectDetails]") {
	STARTUPINFO si{ sizeof(si) };
	PROCESS_INFORMATION pi;
	WCHAR cmd[] = L"cmd.exe /c exit 3";
	REQUIRE(::CreateProcess(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi));
	wil::unique_handle hProcess(pi.hProcess), hThread(pi.hThread);
	REQUIRE(::WaitForSingleObject(hProcess.get(), 10000) == WAIT_OBJECT_0);

	auto text = ObjectDetails::GetDetails(hProcess.get(), L"Process");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, std::format(L"PID: {}", pi.dwProcessId).c_str()));
	CHECK(Contains(text, L"Exit Code: 0x3"));
	CHECK(Contains(text, L"Exited: "));
	CHECK_FALSE(Contains(text, L"Threads: "));
}

TEST_CASE("Details of a thread", "[ObjectDetails]") {
	auto text = ObjectDetails::GetDetails(::GetCurrentThread(), L"Thread");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, std::format(L"TID: {}, PID: {}", ::GetCurrentThreadId(), ::GetCurrentProcessId()).c_str()));
	CHECK(Contains(text, L"Priority: "));
}

TEST_CASE("Details of a token", "[ObjectDetails]") {
	wil::unique_handle hToken;
	REQUIRE(::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, hToken.addressof()));
	auto text = ObjectDetails::GetDetails(hToken.get(), L"Token");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, L"User: "));
	CHECK(Contains(text, L"Type: Primary"));
	CHECK(Contains(text, L"Integrity: "));
	CHECK(Contains(text, L"Elevated: "));
}

TEST_CASE("Details of synchronization objects", "[ObjectDetails]") {
	wil::unique_handle hEvent(::CreateEvent(nullptr, TRUE, TRUE, nullptr));
	CHECK(ObjectDetails::GetDetails(hEvent.get(), L"Event") == L"Type: Manual Reset, Signaled: Yes");

	wil::unique_handle hSemaphore(::CreateSemaphore(nullptr, 2, 5, nullptr));
	CHECK(ObjectDetails::GetDetails(hSemaphore.get(), L"Semaphore") == L"Count: 2, Maximum: 5");

	wil::unique_handle hMutex(::CreateMutex(nullptr, FALSE, nullptr));
	CHECK(ObjectDetails::GetDetails(hMutex.get(), L"Mutant") == L"Held: No");
	::WaitForSingleObject(hMutex.get(), 0);
	auto text = ObjectDetails::GetDetails(hMutex.get(), L"Mutant");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, L"Held: Yes"));
	CHECK(Contains(text, std::format(L"TID {}", ::GetCurrentThreadId()).c_str()));
	::ReleaseMutex(hMutex.get());
}

TEST_CASE("Details of a handle in another process", "[ObjectDetails]") {
	// duplicated from "another" process by PID (here: this one)
	wil::unique_handle hEvent(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
	CHECK(ObjectDetails::GetDetails(hEvent.get(), ::GetCurrentProcessId(), L"Event") == L"Type: Auto Reset, Signaled: No");
}

TEST_CASE("Details of files", "[ObjectDetails]") {
	SECTION("a disk file") {
		WCHAR path[MAX_PATH];
		::GetModuleFileName(nullptr, path, _countof(path));
		wil::unique_hfile file(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(file);
		LARGE_INTEGER size;
		REQUIRE(::GetFileSizeEx(file.get(), &size));
		auto text = ObjectDetails::GetDetails(file.get(), L"File");
		INFO(CW2A(text).m_psz);
		CHECK(Contains(text, L"Device: Disk,"));
		CHECK(Contains(text, std::format(L"Size: {} bytes", size.QuadPart).c_str()));
		CHECK(Contains(text, L"Modified: "));
		CHECK(Contains(text, L"I/O: Synchronous"));
	}

	SECTION("a directory, opened for asynchronous I/O") {
		WCHAR path[MAX_PATH];
		::GetSystemDirectory(path, _countof(path));
		wil::unique_hfile dir(::CreateFile(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
			OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr));
		REQUIRE(dir);
		auto text = ObjectDetails::GetDetails(dir.get(), L"File");
		INFO(CW2A(text).m_psz);
		CHECK(Contains(text, L"Directory: Yes"));
		CHECK(Contains(text, L"I/O: Asynchronous"));
	}

	SECTION("both ends of a named pipe") {
		auto name = std::format(L"\\\\.\\pipe\\ObjExpTests_{}", ::GetCurrentProcessId());
		wil::unique_hfile server(::CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 3, 4096, 4096, 0, nullptr));
		REQUIRE(server);
		wil::unique_hfile client(::CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(client);
		DWORD written;
		REQUIRE(::WriteFile(client.get(), "hello", 5, &written, nullptr));

		auto text = ObjectDetails::GetDetails(server.get(), L"File");
		INFO(CW2A(text).m_psz);
		CHECK(Contains(text, L"Device: Named Pipe, End: Server, State: Connected, Instances: 1 of 3, Available: 5 bytes"));
		CHECK(Contains(ObjectDetails::GetDetails(client.get(), L"File"), L"End: Client"));
	}

	SECTION("a synchronous pipe with a pending read doesn't block the query") {
		auto name = std::format(L"\\\\.\\pipe\\ObjExpTests_Blocked_{}", ::GetCurrentProcessId());
		wil::unique_hfile server(::CreateNamedPipe(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE, 1, 4096, 4096, 0, nullptr));
		REQUIRE(server);
		wil::unique_hfile client(::CreateFile(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(client);
		std::thread reader([&] {
			char buffer[8];
			DWORD read;
			::ReadFile(client.get(), buffer, sizeof(buffer), &read, nullptr);
			});
		::Sleep(200);	// let the read start

		auto start = ::GetTickCount64();
		auto text = ObjectDetails::GetDetails(client.get(), L"File", 0x0BADF11E);
		CHECK(::GetTickCount64() - start < 2000);
		CHECK(text.IsEmpty());

		DWORD written;
		::WriteFile(server.get(), "done", 4, &written, nullptr);
		reader.join();
	}
}

TEST_CASE("Details of the window station and desktop", "[ObjectDetails]") {
	auto text = ObjectDetails::GetDetails(::GetProcessWindowStation(), L"WindowStation");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, L"Interactive: "));
	CHECK(Contains(text, L"Desktops: "));

	text = ObjectDetails::GetDetails(::GetThreadDesktop(::GetCurrentThreadId()), L"Desktop");
	INFO(CW2A(text).m_psz);
	CHECK(Contains(text, L"Heap: "));

	// through the handle owner's process (this one, in this session)
	CHECK(ObjectDetails::GetDetails(::GetProcessWindowStation(), ::GetCurrentProcessId(), L"WindowStation") ==
		ObjectDetails::GetDetails(::GetProcessWindowStation(), L"WindowStation"));
}

TEST_CASE("Types without details", "[ObjectDetails]") {
	wil::unique_handle hEvent(::CreateEvent(nullptr, FALSE, FALSE, nullptr));
	CHECK(ObjectDetails::GetDetails(hEvent.get(), L"Directory").IsEmpty());
}
