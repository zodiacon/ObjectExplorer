#include "pch.h"
#include "DiaHelper.h"

DiaSymbol DiaSymbol::Empty;

DiaSymbol::DiaSymbol(IDiaSymbol* sym) : m_spSym(sym) {
}

DiaSymbol::operator bool() const {
	return m_spSym != nullptr;
}

uint32_t DiaSymbol::TimeStamp() const {
	DWORD ts = 0;
	m_spSym->get_timeStamp(&ts);
	return ts;
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
	return S_OK == m_spSym->get_undecoratedName(&name) ? name.m_str : L"";
}

std::wstring DiaSymbol::CompilerName() const {
	CComBSTR name;
	return S_OK == m_spSym->get_compilerName(&name) ? name.m_str : L"";
}

std::wstring DiaSymbol::SourceFileName() const {
	CComBSTR name;
	return S_OK == m_spSym->get_sourceFileName(&name) ? name.m_str : L"";
}

std::wstring DiaSymbol::ObjectFileName() const {
	CComBSTR name;
	return S_OK == m_spSym->get_objectFileName(&name) ? name.m_str : L"";
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

std::wstring DiaSymbol::LibraryName() const {
	CComBSTR name;
	return S_OK == m_spSym->get_libraryName(&name) ? name.m_str : L"";
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

DiaSymbol DiaSymbol::ArrayIndexType() const {
	CComPtr<IDiaSymbol> sym;
	m_spSym->get_arrayIndexType(&sym);
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

CodeLanguage DiaSymbol::Language() const {
	DWORD lang;
	return S_OK == m_spSym->get_language(&lang) ? CodeLanguage(lang) : CodeLanguage::Unknown;
}

uint32_t DiaSymbol::AddressSection() const {
	ATLASSERT(Location() == LocationKind::Static);
	DWORD address = 0;
	m_spSym->get_addressSection(&address);
	return address;
}

uint32_t DiaSymbol::AddressOffset() const {
	ATLASSERT(Location() == LocationKind::Static);
	DWORD address = 0;
	m_spSym->get_addressOffset(&address);
	return address;
}

uint64_t DiaSymbol::VirtualAddress() const {
	ATLASSERT(Location() == LocationKind::Static);
	ULONGLONG address = 0;
	m_spSym->get_virtualAddress(&address);
	return address;
}

uint32_t DiaSymbol::Slot() const {
	ATLASSERT(Location() == LocationKind::Slot);
	DWORD slot = 0;
	m_spSym->get_slot(&slot);
	return slot;
}

uint32_t DiaSymbol::Signature() const {
	DWORD sig = 0;
	m_spSym->get_signature(&sig);
	return sig;
}

uint32_t DiaSymbol::OffsetInUdt() const {
	DWORD offset = 0;
	m_spSym->get_offsetInUdt(&offset);
	return offset;
}

GUID DiaSymbol::Guid() const {
	GUID guid;
	return S_OK == m_spSym->get_guid(&guid) ? guid : GUID_NULL;
}

bool DiaSymbol::IsConst() const {
	BOOL b = FALSE;
	m_spSym->get_constType(&b);
	return b;
}

uint32_t DiaSymbol::Count() const {
	DWORD count = 0;
	m_spSym->get_count(&count);
	return count;
}

VARIANT DiaSymbol::Value() const {
	VARIANT value;
	::VariantClear(&value);
	m_spSym->get_value(&value);
	return value;
}

uint32_t DiaSymbol::TypeId() const {
	DWORD id = 0;
	m_spSym->get_typeId(&id);
	return id;
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
	auto fields = FindChildren(name.data(), SymbolTag::Data, options);
	return fields.size() == 1 ? fields[0].Offset() : -1;
}

std::vector<DiaSymbol> DiaSymbol::FindChildren(PCWSTR name, SymbolTag tag, CompareOptions options) const {
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

bool DiaSymbol::IsVolatile() const {
	BOOL b = FALSE;
	m_spSym->get_volatileType(&b);
	return b;
}

bool DiaSymbol::IsVirtual() const {
	BOOL virt = false;
	m_spSym->get_virtual(&virt);
	return virt;
}

bool DiaSymbol::IsIntrinsic() const {
	BOOL virt = false;
	m_spSym->get_intrinsic(&virt);
	return virt;
}

bool DiaSymbol::IsIntroVirtual() const {
	BOOL virt = false;
	m_spSym->get_intro(&virt);
	return virt;
}

bool DiaSymbol::IsEditAndContinueEnabled() const {
	BOOL b = false;
	m_spSym->get_editAndContinueEnabled(&b);
	return b;
}

bool DiaSymbol::IsCompilerGenerated() const {
	BOOL b = false;
	m_spSym->get_compilerGenerated(&b);
	return b;
}

bool DiaSymbol::IsUnalignedType() const {
	BOOL b = false;
	m_spSym->get_unalignedType(&b);
	return b;
}

bool DiaSymbol::IsPureVirtual() const {
	BOOL b = FALSE;
	m_spSym->get_pure(&b);
	return b;
}

bool DiaSymbol::IsPacked() const {
	BOOL b = FALSE;
	m_spSym->get_packed(&b);
	return b;
}

bool DiaSymbol::IsReference() const {
	BOOL b = FALSE;
	m_spSym->get_reference(&b);
	return b;
}

bool DiaSymbol::IsNested() const {
	BOOL b = FALSE;
	m_spSym->get_nested(&b);
	return b;
}

bool DiaSymbol::IsCode() const {
	BOOL b = FALSE;
	m_spSym->get_code(&b);
	return b;
}

bool DiaSymbol::IsFunction() const {
	BOOL b = FALSE;
	m_spSym->get_function(&b);
	return b;
}

bool DiaSymbol::IsScoped() const {
	BOOL b = FALSE;
	m_spSym->get_scoped(&b);
	return b;
}

bool DiaSymbol::HasConstructor() const {
	BOOL b = FALSE;
	m_spSym->get_constructor(&b);
	return b;
}

bool DiaSymbol::HasNestedTypes() const {
	BOOL b = FALSE;
	m_spSym->get_hasNestedTypes(&b);
	return b;
}

bool DiaSymbol::IsManaged() const {
	BOOL b = FALSE;
	m_spSym->get_managed(&b);
	return b;
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
