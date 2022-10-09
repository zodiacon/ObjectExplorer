#pragma once

#include <string>
#include <vector>
#include <atlcomcli.h>

struct IDiaSymbol;

class DiaSymbol {
	friend class DiaSession;
public:
    operator bool() const;

    std::wstring Name() const;
    std::wstring UndecoratedName() const;
    uint32_t Id() const;
    uint32_t Age() const;
    int32_t Offset() const;
    AccessMode Access() const;
    DiaSymbol ClassParent() const;
    DiaSymbol LexicalParent() const;
    DiaSymbol Type() const;
    LocationKind Location() const;
    SymbolTag Tag() const;
    uint64_t Length() const;
    DataItemKind Kind() const;
    bool IsConst() const;
    bool IsVolatile() const;
    bool IsVirtual() const;
    uint32_t Count() const;
    VARIANT Value() const;
    uint32_t TypeId() const;
    uint32_t BitPosition() const;
    UdtType UdtKind() const;
    SimpleType Simple() const;
    std::wstring TypeName() const;
    DiaSymbol ObjectPointerType() const;
    uint32_t ClassParentId() const;
    int32_t GetFieldOffset(std::wstring_view name, CompareOptions options = CompareOptions::None) const;

    std::vector<DiaSymbol> FindChildren(SymbolTag tag = SymbolTag::Null, PCWSTR name = nullptr,
        CompareOptions options = CompareOptions::None) const;

    static std::wstring SimpleTypeToString(SimpleType type, DWORD len);

protected:
	DiaSymbol(IDiaSymbol* sym = nullptr);

	CComPtr<IDiaSymbol> m_spSym;

public:
    static DiaSymbol Empty;
};

