#include "pch.h"
#include "SymbolToTreeView.h"
#include "TreeListView.h"

void SymbolToTreeView::FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, PVOID address) {
	for (auto member : sym.FindChildren()) {
		int image = 2;
		if (member.Location() == LocationKind::BitField)
			image = 4;
		else {
			switch (member.Type().Tag()) {
				case SymbolTag::UDT: 
					image = member.Type().UdtKind() == UdtType::Union ? 1 : 0;
					break;
				case SymbolTag::Enum: 
					image = 3; 
					break;
			}
		}
		auto hItem = tv.GetTreeControl().InsertItem(member.Name().c_str(), image, image, hRoot, TVI_LAST);
		tv.SetSubItemText(hItem, 1, member.TypeName().c_str());
		if (member.Type().Tag() == SymbolTag::UDT) {
			FillTreeView(tv, hItem, member.Type());
		}
	}
}
