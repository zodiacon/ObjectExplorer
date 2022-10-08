#include "pch.h"
#include "SymbolManager.h"

SymbolManager& SymbolManager::Get() {
	static SymbolManager sm;
	return sm;
}

SymbolManager::operator bool() const {
	return (bool)m_session;
}

const DiaSymbol SymbolManager::GetSymbol(PCWSTR name) const {
	auto symbols = m_session.FindChildren(m_session.GlobalScope(), SymbolTag::UDT, name);
	return symbols.empty() ? DiaSymbol::Empty : DiaSymbol(symbols[0]);
}

SymbolManager::SymbolManager() {
	WCHAR path[MAX_PATH];
	::GetSystemDirectory(path, _countof(path));
	wcscat_s(path, L"\\ntoskrnl.exe");
	m_session.OpenImage(path);
}
