#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "AppSettings.h"

namespace {
	// sets _NT_SYMBOL_PATH for the scope, restoring it afterwards
	struct SymbolPathVariable {
		explicit SymbolPathVariable(PCWSTR value) {
			WCHAR old[4096];
			auto len = ::GetEnvironmentVariable(L"_NT_SYMBOL_PATH", old, _countof(old));
			m_Had = len > 0 && len < _countof(old);
			if (m_Had)
				m_Old = old;
			::SetEnvironmentVariable(L"_NT_SYMBOL_PATH", value);
		}
		~SymbolPathVariable() {
			::SetEnvironmentVariable(L"_NT_SYMBOL_PATH", m_Had ? m_Old.c_str() : nullptr);
		}
		bool m_Had;
		std::wstring m_Old;
	};

	std::wstring Build(bool useServer, PCWSTR url, PCWSTR cache, PCWSTR extra, bool useEnv) {
		return AppSettings::BuildSymbolSearchPath(useServer, url, cache, extra, useEnv);
	}
}

TEST_CASE("Nothing selected gives an empty path", "[SymbolPath]") {
	SymbolPathVariable env(L"c:\\env");
	CHECK(Build(false, L"https://server", L"c:\\cache", L"", false).empty());
}

TEST_CASE("The symbol server with a cache", "[SymbolPath]") {
	SymbolPathVariable env(nullptr);
	CHECK(Build(true, L"https://server/symbols", L"c:\\cache", L"", true) == L"srv*c:\\cache*https://server/symbols");
	// no cache: DIA's default (the server only)
	CHECK(Build(true, L"https://server/symbols", L"  ", L"", true) == L"srv*https://server/symbols");
	// no URL: no server
	CHECK(Build(true, L"", L"c:\\cache", L"", true).empty());
}

TEST_CASE("Environment variables in the cache are expanded", "[SymbolPath]") {
	SymbolPathVariable env(nullptr);
	WCHAR expanded[MAX_PATH];
	::ExpandEnvironmentStrings(L"%LOCALAPPDATA%\\ObjectExplorer\\Symbols", expanded, _countof(expanded));
	CHECK(Build(true, AppSettings::DefaultSymbolServerUrl, AppSettings::DefaultSymbolCache, L"", false) ==
		std::wstring(L"srv*") + expanded + L"*" + AppSettings::DefaultSymbolServerUrl);
}

TEST_CASE("Local paths come before the server", "[SymbolPath]") {
	SymbolPathVariable env(L" c:\\env;srv*c:\\envcache*https://env ;");
	CHECK(Build(true, L"https://server", L"c:\\cache", L"; c:\\pdbs;d:\\more ;", true) ==
		L"c:\\pdbs;d:\\more;c:\\env;srv*c:\\envcache*https://env;srv*c:\\cache*https://server");
}

TEST_CASE("_NT_SYMBOL_PATH only when asked", "[SymbolPath]") {
	SymbolPathVariable env(L"c:\\env");
	CHECK(Build(false, L"", L"", L"c:\\pdbs", false) == L"c:\\pdbs");
	CHECK(Build(false, L"", L"", L"c:\\pdbs", true) == L"c:\\pdbs;c:\\env");
}
