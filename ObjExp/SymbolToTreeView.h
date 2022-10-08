#pragma once

#include "DiaHelper.h"

struct ITreeViewFillCallback {
	virtual int GetImageForSymbol(DiaSymbol const& sym) = 0;
	virtual std::wstring GetValue(DiaSymbol const& sym) = 0;
};

class CTreeListView;

class SymbolToTreeView {
public:
	static void FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, ITreeViewFillCallback* cb = nullptr);
};

