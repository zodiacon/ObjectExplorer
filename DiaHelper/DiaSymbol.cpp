#include "pch.h"
#include "DiaHelper.h"
#include "DiaSymbol.h"

DiaSymbol DiaSymbol::Empty;

DiaSymbol::DiaSymbol(IDiaSymbol* sym) : m_spSym(sym) {
}

DiaSymbol::operator bool() const {
	return m_spSym != nullptr;
}

std::wstring DiaSymbol::Name() const {
	CComBSTR name;
	if (S_OK == m_spSym->get_name(&name) && name.Length() > 0)
		return name.m_str;

	auto type = Type();
	switch (Tag()) {
		case SymbolTag::PointerType:
			return type.Name() + L"*";
		case SymbolTag::BaseType:
			return SimpleTypeToString(Simple(), type ? (DWORD)type.Length() : 0);
		case SymbolTag::ArrayType:
			return type.Name() + std::format(L"[{}]", Count());
		case SymbolTag::FunctionType:
			return Type().Name();
	}
	return L"";
}

std::wstring DiaSymbol::UndecoratedName() const {
	CComBSTR name;
	m_spSym->get_undecoratedName(&name);
	return name.Length() == 0 ? L"" : name.m_str;
}

uint32_t DiaSymbol::Id() const {
	uint32_t id = 0;
	m_spSym->get_symIndexId((DWORD*)&id);
	return id;
}

uint32_t DiaSymbol::Age() const {
	uint32_t age = 0;
	m_spSym->get_age((DWORD*)&age);
	return age;
}

int32_t DiaSymbol::Offset() const {
	int32_t offset;
	m_spSym->get_offset((LONG*)&offset);
	return offset;
}

AccessMode DiaSymbol::Access() const {
	DWORD access = 0;
	m_spSym->get_access(&access);
	return AccessMode(access);
}

DiaSymbol DiaSymbol::ClassParent() const {
	CComPtr<IDiaSymbol> sym;
	m_spSym->get_classParent(&sym);
	return DiaSymbol(sym);
}

DiaSymbol DiaSymbol::LexicalParent() const {
	CComPtr<IDiaSymbol> sym;
	m_spSym->get_lexicalParent(&sym);
	return DiaSymbol(sym);
}

DiaSymbol DiaSymbol::Type() const {
	CComPtr<IDiaSymbol> sym;
	m_spSym->get_type(&sym);
	return DiaSymbol(sym);
}

LocationKind DiaSymbol::Location() const {
	DWORD loc = 0;
	m_spSym->get_locationType(&loc);
	return static_cast<LocationKind>(loc);
}

SymbolTag DiaSymbol::Tag() const {
	DWORD tag = 0;
	m_spSym->get_symTag(&tag);
	return SymbolTag(tag);
}

uint64_t DiaSymbol::Length() const {
	ULONGLONG len;
	m_spSym->get_length(&len);
	return uint64_t(len);
}

DataItemKind DiaSymbol::Kind() const {
	DWORD kind = 0;
	m_spSym->get_dataKind(&kind);
	return static_cast<DataItemKind>(kind);
}

uint32_t DiaSymbol::Count() const {
	DWORD count = 0;
	m_spSym->get_count(&count);
	return count;
}

uint32_t DiaSymbol::BitPosition() const {
	DWORD pos;
	m_spSym->get_bitPosition(&pos);
	return pos;
}

SimpleType DiaSymbol::Simple() const {
	DWORD type;
	return S_OK == m_spSym->get_baseType(&type) ? static_cast<SimpleType>(type) : SimpleType::NoType;
}

std::wstring DiaSymbol::TypeName() const {
	auto type = Type();
	if (!type)
		return L"";

	auto name = type.Name();
	if(Location() == LocationKind::BitField)
		name += std::format(L" Pos {}, {} Bit{}", BitPosition(), Length(), Length() > 1 ? L"s" : L"").c_str();

	if (name.empty())
		DebugBreak();

	return name;
}

DiaSymbol DiaSymbol::ObjectPointerType() const {
	CComPtr<IDiaSymbol> spType;
	m_spSym->get_objectPointerType(&spType);
	return DiaSymbol(spType);
}

uint32_t DiaSymbol::ClassParentId() const {
	DWORD id = 0;
	m_spSym->get_classParentId(&id);
	return id;
}

int32_t DiaSymbol::GetFieldOffset(std::wstring_view name, CompareOptions options) const {
	auto fields = FindChildren(SymbolTag::Data, name.data(), options);
	return fields.size() == 1 ? fields[0].Offset() : -1;
}

std::vector<DiaSymbol> DiaSymbol::FindChildren(SymbolTag tag, PCWSTR name, CompareOptions options) const {
	std::vector<DiaSymbol> symbols;
	CComPtr<IDiaEnumSymbols> spEnum;
	if (SUCCEEDED(m_spSym->findChildren((enum SymTagEnum)tag, name, (DWORD)options, &spEnum))) {
		LONG count = 0;
		spEnum->get_Count(&count);
		ULONG ret;
		for (LONG i = 0; i < count; i++) {
			CComPtr<IDiaSymbol> sym;
			spEnum->Next(1, &sym, &ret);
			ATLASSERT(sym);
			if (sym == nullptr)
				break;
			symbols.push_back(DiaSymbol(sym));
		}
	}
	return symbols;
}

bool DiaSymbol::IsVirtual() const {
	BOOL virt = false;
	m_spSym->get_virtual(&virt);
	return virt;
}

UdtType DiaSymbol::UdtKind() const {
	DWORD kind;
	m_spSym->get_udtKind(&kind);
	return UdtType(kind);
}

std::wstring DiaSymbol::SimpleTypeToString(SimpleType type, DWORD len) {
	switch (type) {
		case SimpleType::Void: return L"Void";
		case SimpleType::Char: return L"Char";
		case SimpleType::WChar: return L"WCHAR";
		case SimpleType::Bit: return L"Bit";
		case SimpleType::Complex: return L"Complex";
		case SimpleType::Int:
			switch (len) {
				case 1:	return L"Int1B";
				case 2: return L"Int2B";
				case 4: return L"Int4B";
				case 8: return L"Int8B";
				default: return L"Int8B";
			}
			break;
		case SimpleType::UInt:
			switch (len) {
				case 1:	return L"UInt1B";
				case 2: return L"UInt2B";
				case 4: return L"UInt4B";
				case 8: return L"UInt8B";
				default: return L"UInt8B";
			}
			break;

		case SimpleType::Bool: return L"Bool";
		case SimpleType::Float: return L"Float";
		case SimpleType::Int4B: return L"Int4B";
		case SimpleType::UInt4B: return L"UInt4B";
		case SimpleType::Hresult: return L"HRESULT";
	}
	//return L"<" + std::to_wstring((int)type) + L" " + std::to_wstring(len) + L">";
	return L"<Unknown>";
}
