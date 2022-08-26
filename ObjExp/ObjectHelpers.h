#pragma once

struct ObjectHelpers abstract final {
	static UINT ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name = nullptr, PCWSTR target = nullptr, DWORD handleCount = 0);
	static std::vector<std::pair<CString, CString>> GetSimpleProps(HANDLE hObject, PCWSTR type, PCWSTR name, PCWSTR target = nullptr);
	static bool IsNamedObjectType(USHORT index);
};

