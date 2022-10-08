#pragma once

#include "DiaHelper.h"

class CTreeListView;

class SymbolToTreeView {
public:
	static void FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, PVOID address = nullptr);
};

