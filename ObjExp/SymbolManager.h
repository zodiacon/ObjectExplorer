#pragma once

#include "DiaHelper.h"

class SymbolManager {
public:
	static SymbolManager& Get();

	operator bool() const;

	const DiaSymbol GetSymbol(PCWSTR name) const;
	DiaSession& Session() const;

	std::wstring ReadUnicodeString(PVOID address);

	// loads the kernel's symbols again, with the symbol settings
	bool Reload();
	// the symbol settings changed: reload when the symbols are next used (loading may download the PDB)
	static void Invalidate();

private:
	SymbolManager();

	DiaSession m_session;
	// static: invalidating doesn't create (and load) the symbols
	inline static bool s_Stale{ false };
};

