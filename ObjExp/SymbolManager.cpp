#include "pch.h"
#include "SymbolManager.h"
#include "DbgDriver.h"
#include "AppSettings.h"

SymbolManager& SymbolManager::Get() {
	static SymbolManager sm;
	if (s_Stale) {
		CWaitCursor wait;
		sm.Reload();
	}
	return sm;
}

void SymbolManager::Invalidate() {
	s_Stale = true;
}

SymbolManager::operator bool() const {
	return (bool)m_session;
}

const DiaSymbol SymbolManager::GetSymbol(PCWSTR name) const {
	if (!m_session)
		return DiaSymbol::Empty;

	auto symbols = m_session.FindChildren(m_session.GlobalScope(), name);
	return symbols.empty() ? DiaSymbol::Empty : DiaSymbol(symbols[0]);
}

SymbolManager::SymbolManager() {
	Reload();
}

bool SymbolManager::Reload() {
	s_Stale = false;
	m_session.Close();
	m_session.SetSymbolPath(AppSettings::Get().BuildSymbolSearchPath().c_str());
	WCHAR path[MAX_PATH];
	::GetSystemDirectory(path, _countof(path));
	wcscat_s(path, L"\\ntoskrnl.exe");
	return m_session.OpenImage(path);
}

std::wstring SymbolManager::ReadUnicodeString(PVOID address) {
	static auto bufferOffset = 0;
	if (bufferOffset == 0) {
		auto symbols = m_session.FindChildren(m_session.GlobalScope(), L"_UNICODE_STRING", SymbolTag::UDT);
		if (symbols.size() != 1)
			return L"";
		bufferOffset = symbols[0].GetFieldOffset(L"Buffer");
	}
	if (bufferOffset < 0)
		return L"";

	auto& driver = DbgDriver::Get();
	// whole characters only: an odd length would read past the string's buffer into its terminator
	auto len = driver.ReadVirtual<USHORT>(address) & ~1;
	if (len == 0)
		return L"";

	auto buffer = driver.ReadVirtual<PVOID>((PBYTE)address + bufferOffset);
	if (buffer == nullptr)
		return L"";
	std::wstring result;
	result.resize(len / sizeof(WCHAR));
	return driver.ReadVirtual(buffer, len, result.data()) ? result : L"";
}
