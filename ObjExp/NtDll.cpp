#include "pch.h"
#include "NtDll.h"

namespace NT {
	HWINSTA NTAPI NtUserOpenWindowStation(_In_ POBJECT_ATTRIBUTES attr, _In_ ACCESS_MASK access) {
		static const auto pNtUserOpenWindowStation = (decltype(NT::NtUserOpenWindowStation)*)::GetProcAddress(
			::GetModuleHandle(L"win32u"), "NtUserOpenWindowStation");

		return pNtUserOpenWindowStation ? pNtUserOpenWindowStation(attr, access) : nullptr;
	}
}
