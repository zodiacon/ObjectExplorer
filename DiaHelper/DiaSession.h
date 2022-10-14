#pragma once

#include "dia2.h"
#include "DiaSymbol.h"
#include <atlcomcli.h>
#include <vector>
#include <string>

class DiaSession {
public:
	bool OpenImage(PCWSTR path);
	bool OpenPdb(PCWSTR path);
	void Close();
	std::wstring LastError() const;
	operator bool() const;

	DiaSymbol GlobalScope() const;
	std::vector<DiaSymbol> FindChildren(DiaSymbol const& parent, PCWSTR name = nullptr, SymbolTag tag = SymbolTag::Null, CompareOptions options = CompareOptions::None) const;
	// global scope
	std::vector<DiaSymbol> FindChildren(PCWSTR name = nullptr, SymbolTag tag = SymbolTag::Null, CompareOptions options = CompareOptions::None) const;

private:
	bool OpenCommon(PCWSTR path, bool image);

private:
	CComPtr<IDiaSession> m_spSession;
	CComPtr<IDiaDataSource> m_spSource;
};

