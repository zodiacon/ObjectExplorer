#pragma once

#include "ObjectManager.h"

struct StringHelper abstract final {
	static PCWSTR PoolTypeToString(PoolType type);
	static CString SectionAttributesToString(DWORD value);
};

