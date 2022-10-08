#pragma once

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
	std::vector<DiaSymbol> FindChildren(DiaSymbol const& parent, SymbolTag tag = SymbolTag::Null, PCWSTR name = nullptr,
		CompareOptions options = CompareOptions::None) const;

private:
	bool OpenCommon(PCWSTR path, bool image);

private:
	CComPtr<IDiaSession> m_spSession;
	CComPtr<IDiaDataSource> m_spSource;
};

