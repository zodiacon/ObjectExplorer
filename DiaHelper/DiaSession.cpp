// DiaHelper.cpp : Defines the functions for the static library.
//

#include "pch.h"
#include "DiaHelper.h"

HMODULE g_hDiaDll;

bool DiaSession::OpenImage(PCWSTR path) {
	return OpenCommon(path, true);
}

bool DiaSession::OpenPdb(PCWSTR path) {
	return OpenCommon(path, false);
}

void DiaSession::Close() {
	m_spSession.Release();
}

DiaSession::operator bool() const {
	return m_spSession != nullptr;
}

std::wstring DiaSession::LastError() const {
	CComBSTR text;
	return m_spSource && S_OK == m_spSource->get_lastError(&text) ? text.m_str : L"";
}

DiaSymbol DiaSession::GlobalScope() const {
	if (m_spSession == nullptr)
		return DiaSymbol(nullptr);

	CComPtr<IDiaSymbol> spSym;
	m_spSession->get_globalScope(&spSym);
	return DiaSymbol(spSym);
}

std::vector<DiaSymbol> DiaSession::FindChildren(DiaSymbol const& parent, PCWSTR name, SymbolTag tag, CompareOptions options) const {
	std::vector<DiaSymbol> symbols;
	CComPtr<IDiaEnumSymbols> spEnum;
	if (SUCCEEDED(m_spSession->findChildren(parent.m_spSym, (enum SymTagEnum)tag, name, (DWORD)options, &spEnum))) {
		LONG count = 0;
		spEnum->get_Count(&count);
		ULONG ret;
		for (LONG i = 0; i < count; i++) {
			CComPtr<IDiaSymbol> sym;
			spEnum->Next(1, &sym, &ret);
			ATLASSERT(sym);
			if (sym == nullptr)
				break;
			symbols.push_back(DiaSymbol(sym));
		}
	}
	return symbols;
}

std::vector<DiaSymbol> DiaSession::FindChildren(PCWSTR name, SymbolTag tag, CompareOptions options) const {
	return FindChildren(GlobalScope(), name, tag, options);
}

bool DiaSession::OpenCommon(PCWSTR path, bool image) {
	if (g_hDiaDll == nullptr) {
		WCHAR path[MAX_PATH];
		if (::GetModuleFileName(nullptr, path, _countof(path))) {
			auto bs = wcsrchr(path, L'\\');
			*bs = 0;
			wcscat_s(path, L"\\msdia140.dll");
			g_hDiaDll = ::LoadLibrary(path);
		}
	}
	CComPtr<IDiaDataSource> spSource;
	HRESULT hr;
	if (g_hDiaDll) {
		//
		// implement CoCreateInstance manually to make sure our msdia DLL is loaded
		//
		auto dgco = (decltype(::DllGetClassObject)*)::GetProcAddress(g_hDiaDll, "DllGetClassObject");
		if (!dgco)
			return false;

		CComPtr<IClassFactory> spCF;
		if (FAILED(dgco(__uuidof(DiaSource), __uuidof(IClassFactory), reinterpret_cast<void**>(&spCF))))
			return false;

		hr = spCF->CreateInstance(nullptr, __uuidof(IDiaDataSource), reinterpret_cast<void**>(&spSource));
	}
	else {
		hr = spSource.CoCreateInstance(__uuidof(DiaSource));
	}
	if (FAILED(hr))
		return false;
	if (image)
		hr = spSource->loadDataForExe(path, nullptr, nullptr);
	else
		hr = spSource->loadDataFromPdb(path);
	if (FAILED(hr))
		return false;

	CComPtr<IDiaSession> spSession;
	hr = spSource->openSession(&spSession);
	if (FAILED(hr))
		return false;

	m_spSession = spSession;
	m_spSource = spSource;
	return true;
}
