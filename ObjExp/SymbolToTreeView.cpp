#include "pch.h"
#include "SymbolToTreeView.h"
#include "TreeListView.h"
#include "DbgDriver.h"

void SymbolToTreeView::FillTreeView(CTreeListView& tv, HTREEITEM hRoot, DiaSymbol const& sym, PVOID address) {
	auto& driver = DbgDriver::Get();

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
		auto hItem = tv.GetTreeControl().InsertItem(std::format(L"{:3X}: {}", member.Offset(), member.Name()).c_str(), 
			image, image, hRoot, TVI_LAST);
		tv.SetSubItemText(hItem, 1, member.TypeName().c_str());

		auto type = member.Type();

		//
		// add values if Debug Driver is available
		//
		if (address && driver) {
			ULONG64 value = 0;
			if (member.Location() == LocationKind::BitField) {
				if (driver.ReadVirtual((PBYTE)address + member.Offset(), (ULONG)type.Length(), &value)) {
					tv.SetSubItemText(hItem, 2, std::format(L"0x{:X}", (value >> member.BitPosition()) & ((1 << (ULONG)member.Length()) - 1)).c_str(), TLVIFMT_RIGHT);
				}
			}
			else if (type.Simple() != SimpleType::NoType || type.Tag() == SymbolTag::Enum || type.Tag() == SymbolTag::PointerType) {
				ATLASSERT(type.Length());
				if (driver.ReadVirtual((PBYTE)address + member.Offset(), (ULONG)type.Length(), &value))
					tv.SetSubItemText(hItem, 2, std::format(L"0x{:X}", value).c_str(), TLVIFMT_RIGHT);
			}
		}

		if (member.Type().Tag() == SymbolTag::UDT) {
			if(address)
				tv.SetSubItemText(hItem, 2, std::format(L"0x{:X}", (DWORD64)((PBYTE)address + member.Offset())).c_str(), TLVIFMT_RIGHT);
			FillTreeView(tv, hItem, member.Type(), (PBYTE)address + member.Offset());
		}
	}
}
