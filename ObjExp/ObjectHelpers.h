#pragma once

struct ObjectHelpers abstract final {
	static UINT ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name = nullptr);
};

