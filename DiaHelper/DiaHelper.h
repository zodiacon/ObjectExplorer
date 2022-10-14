#pragma once

enum class AccessMode {
    Private = 1,
    Protected,
    Public
};

enum class LocationKind {
    Null,
    Static,
    TLS,
    RegRel,
    ThisRel,
    Enregistered,
    BitField,
    Slot,
    IlRel,
    MetaData,
    Constant,
    RegRelAliasIndir,
    Max
};

enum class DataItemKind {
    Unknown,
    Local,
    StaticLocal,
    Param,
    ObjectPtr,
    FileStatic,
    Global,
    Member,
    StaticMember,
    Constant
};

enum class UdtType {
    Struct,
    Class,
    Union,
};

enum class SimpleType {
    NoType = 0,
    Void = 1,
    Char = 2,
    WChar = 3,
    Int = 6,
    UInt = 7,
    Float = 8,
    BCD = 9,
    Bool = 10,
    Int4B = 13,
    UInt4B = 14,
    Currency = 25,
    Date = 26,
    Variant = 27,
    Complex = 28,
    Bit = 29,
    BSTR = 30,
    Hresult = 31,
    Char16 = 32,  // char16_t
    Char32 = 33,  // char32_t
    Char8 = 34,  // char8_t
};

enum class CodeLanguage {
    C = 0x00,
    Cpp = 0x01,
    Fortran = 0x02,
    Masm = 0x03,
    Pascal = 0x04,
    Basic = 0x05,
    Cobol = 0x06,
    Link = 0x07,
    CVTRES = 0x08,
    CVTPGD = 0x09,
    Unknown = 0xffff
};

enum class PlatformType {
    IA_8080 = 0x00,
    IA_8086 = 0x01,
    IA_80286 = 0x02,
    IA_80386 = 0x03,
    IA_80486 = 0x04,
    PENTIUM = 0x05,
    PENTIUMII = 0x06,
    PENTIUMPRO = PENTIUMII,
    PENTIUMIII = 0x07,
    MIPS = 0x10,
    MIPSR4000 = MIPS,
    MIPS16 = 0x11,
    MIPS32 = 0x12,
    MIPS64 = 0x13,
    MIPSI = 0x14,
    MIPSII = 0x15,
    MIPSIII = 0x16,
    MIPSIV = 0x17,
    MIPSV = 0x18,
    M68000 = 0x20,
    M68010 = 0x21,
    M68020 = 0x22,
    M68030 = 0x23,
    M68040 = 0x24,
    ALPHA = 0x30,
    ALPHA_21064 = 0x30,
    ALPHA_21164 = 0x31,
    ALPHA_21164A = 0x32,
    ALPHA_21264 = 0x33,
    ALPHA_21364 = 0x34,
    PPC601 = 0x40,
    PPC603 = 0x41,
    PPC604 = 0x42,
    PPC620 = 0x43,
    SH3 = 0x50,
    SH3E = 0x51,
    SH3DSP = 0x52,
    SH4 = 0x53,
    ARM3 = 0x60,
    ARM4 = 0x61,
    ARM4T = 0x62,
    OMNI = 0x70,
    IA64 = 0x80,
    CEE = 0x90
};

enum class CompareOptions {
    None = 0,
    CaseSensitive = 0x1,
    CaseInsensitive = 0x2,
    FNameExt = 0x4,
    RegularExpression = 0x8,
    UndecoratedName = 0x10,
    CaseInRegularExpression = (RegularExpression | CaseInsensitive)
};
DEFINE_ENUM_FLAG_OPERATORS(CompareOptions);

enum class SymbolTag {
    Null,
    Exe,
    Compiland,
    CompilandDetails,
    CompilandEnv,
    Function,
    Block,
    Data,
    Annotation,
    Label,
    PublicSymbol,
    UDT,
    Enum,
    FunctionType,
    PointerType,
    ArrayType,
    BaseType,
    Typedef,
    BaseClass,
    Friend,
    FunctionArgType,
    FuncDebugStart,
    FuncDebugEnd,
    UsingNamespace,
    VTableShape,
    VTable,
    Custom,
    Thunk,
    CustomType,
    ManagedType,
    Dimension,
    CallSite,
    InlineSite,
    BaseInterface,
    VectorType,
    MatrixType,
    HLSLType,
    Caller,
    Callee,
    Export,
    HeapAllocationSite,
    CoffGroup,
    Inlinee,
    Max
};

#include "dia2.h"
#include "DiaSession.h"
#include "DiaSymbol.h"
