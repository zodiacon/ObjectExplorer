#pragma once

#include "DiaHelper.h"

class CTreeListView;

class SymbolToTreeView {
public:
	static void FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, PVOID address = nullptr);
	static std::wstring GetSpecialValue(DiaSymbol const& field, DiaSymbol const& type, PVOID address, bool& isString);
};

