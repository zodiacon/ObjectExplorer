#pragma once

#include "DiaHelper.h"

class SymbolManager {
public:
	static SymbolManager& Get();

	operator bool() const;

	const DiaSymbol GetSymbol(PCWSTR name) const;
	DiaSession& Session() const;

	std::wstring ReadUnicodeString(PVOID address);

private:
	SymbolManager();

	DiaSession m_session;
};

