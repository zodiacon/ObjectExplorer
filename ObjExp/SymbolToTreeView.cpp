#include "pch.h"
#include "SymbolToTreeView.h"
#include "TreeListView.h"

void SymbolToTreeView::FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, ITreeViewFillCallback* cb) {
	for (auto member : sym.FindChildren()) {
		//auto name = member.Name() + L" (" + member.TypeName() + L")";
		int image = -1;
		if (cb)
			image = cb->GetImageForSymbol(sym);
		auto hItem = tv.GetTreeControl().InsertItem(member.Name().c_str(), image, image, hRoot, TVI_LAST);
		tv.SetSubItemText(hItem, 1, member.TypeName().c_str());
		if(cb)
			tv.SetSubItemText(hItem, 2, cb->GetValue(member).c_str());
		if (member.Type().Tag() == SymbolTag::UDT) {
			FillTreeView(tv, hItem, member.Type());
		}
	}
}
