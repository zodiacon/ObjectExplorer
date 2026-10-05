#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "ProcessHelper.h"
#include <thread>

namespace {
	CString ModulePath() {
		WCHAR path[MAX_PATH];
		::GetModuleFileName(nullptr, path, _countof(path));
		return path;
	}

	CString ModuleName() {
		auto path = ModulePath();
		return path.Mid(path.ReverseFind(L'\\') + 1);
	}

	// e.g. \Device\HarddiskVolume3 for C:
	CString NtDeviceOf(PCWSTR drive) {
		WCHAR device[MAX_PATH];
		return ::QueryDosDevice(drive, device, _countof(device)) ? CString(device) : CString();
	}
}

TEST_CASE("The name of the current process", "[ProcessHelper]") {
	auto pid = ::GetCurrentProcessId();
	CHECK(ProcessHelper::GetProcessName(pid).CompareNoCase(ModuleName()) == 0);
	CHECK(ProcessHelper::GetProcessName2(pid).CompareNoCase(ModuleName()) == 0);
	CHECK(ProcessHelper::GetFullProcessImageName(pid).CompareNoCase(ModulePath()) == 0);
}

TEST_CASE("A process that can't be opened gets its name from a snapshot", "[ProcessHelper]") {
	// the System process has no image path, so its name comes from the process snapshot
	CHECK(ProcessHelper::GetProcessName(4) == L"System");
}

TEST_CASE("An unknown process", "[ProcessHelper]") {
	CHECK(ProcessHelper::GetProcessName(0xFFFFFFF0) == L"<Unknown>");
	CHECK(ProcessHelper::GetProcessName2(0xFFFFFFF0) == L"<Unknown>");
}

TEST_CASE("Process names can be looked up from several threads", "[ProcessHelper]") {
	auto pid = ::GetCurrentProcessId();
	auto expected = ModuleName();
	std::atomic<int> mismatches = 0;
	std::vector<std::thread> threads;
	for (int t = 0; t < 8; t++) {
		threads.emplace_back([&, t] {
			for (int i = 0; i < 200; i++) {
				// unknown IDs make the threads refresh the snapshot cache concurrently
				ProcessHelper::GetProcessName(0xFFFF0000 + t * 1000 + i);
				if (ProcessHelper::GetProcessName(pid).CompareNoCase(expected) != 0)
					mismatches++;
			}
			});
	}
	for (auto& thread : threads)
		thread.join();
	CHECK(mismatches == 0);
}

TEST_CASE("The user of the current process", "[ProcessHelper]") {
	WCHAR user[256], domain[256];
	REQUIRE(::GetEnvironmentVariable(L"USERNAME", user, _countof(user)));
	REQUIRE(::GetEnvironmentVariable(L"USERDOMAIN", domain, _countof(domain)));
	auto expected = CString(domain) + L"\\" + user;
	CHECK(CString(ProcessHelper::GetUserName(::GetCurrentProcessId()).c_str()).CompareNoCase(expected) == 0);
}

TEST_CASE("NT device paths are converted to drive letters", "[ProcessHelper]") {
	WCHAR system[MAX_PATH];
	::GetSystemDirectory(system, _countof(system));
	CString drive(system, 2);	// e.g. C:
	auto device = NtDeviceOf(drive);
	REQUIRE(!device.IsEmpty());

	CHECK(ProcessHelper::GetDosNameFromNtName(device + L"\\Windows\\notepad.exe") == std::wstring(drive + L"\\Windows\\notepad.exe"));
	CHECK(ProcessHelper::GetDosNameFromNtName(device) == std::wstring(drive));
	CHECK(ProcessHelper::GetDosNameFromNtName(L"\\Device\\NoSuchDevice\\x").empty());

	SECTION("a device whose name starts with another's isn't confused with it") {
		// e.g. \Device\HarddiskVolume3 and \Device\HarddiskVolume30
		auto longer = device + L"9";
		bool exists = false;
		for (WCHAR letter = L'A'; letter <= L'Z'; letter++) {
			WCHAR other[] = { letter, L':', 0 };
			if (NtDeviceOf(other) == longer)
				exists = true;
		}
		if (!exists)
			CHECK(ProcessHelper::GetDosNameFromNtName(longer + L"\\Windows").empty());
	}
}
