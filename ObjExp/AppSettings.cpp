#include "pch.h"
#include "AppSettings.h"

namespace {
	std::wstring Trim(std::wstring const& s, std::wstring const& chars = L" \t\r\n") {
		auto first = s.find_first_not_of(chars);
		if (first == std::wstring::npos)
			return {};
		return s.substr(first, s.find_last_not_of(chars) - first + 1);
	}
}

std::wstring AppSettings::BuildSymbolSearchPath() const {
	return BuildSymbolSearchPath(SymbolServerEnabled() != 0, SymbolServerUrl(), SymbolCache(), SymbolExtraPaths(), SymbolUseEnvPath() != 0);
}

std::wstring AppSettings::BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
	std::wstring const& extra, bool useEnv) {
	std::wstring path;
	auto add = [&](std::wstring const& part) {
		if (part.empty())
			return;
		if (!path.empty())
			path += L";";
		path += part;
	};

	// local locations first, so the server is only consulted when they have nothing
	add(Trim(extra, L" \t\r\n;"));
	if (useEnv) {
		WCHAR env[4096];
		auto len = ::GetEnvironmentVariable(L"_NT_SYMBOL_PATH", env, _countof(env));
		if (len > 0 && len < _countof(env))
			add(Trim(env, L" \t\r\n;"));
	}
	auto server = Trim(url);
	if (useServer && !server.empty()) {
		auto dir = Trim(cache);
		if (!dir.empty()) {
			WCHAR expanded[MAX_PATH];
			auto len = ::ExpandEnvironmentStrings(dir.c_str(), expanded, _countof(expanded));
			if (len > 0 && len <= _countof(expanded))
				dir = expanded;
		}
		add(dir.empty() ? L"srv*" + server : L"srv*" + dir + L"*" + server);
	}
	return path;
}
