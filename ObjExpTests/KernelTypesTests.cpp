#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "ObjectHelpers.h"
#include "ObjectManager.h"
#include "SymbolManager.h"

// the object types whose properties show a kernel structure (the Object tab)

TEST_CASE("Kernel structures are mapped from existing object types", "[KernelTypes]") {
	ObjectManager::EnumTypes();
	auto& types = ObjectManager::GetObjectTypes();
	for (auto& [type, structure] : ObjectHelpers::KernelTypes) {
		INFO("type " << CW2A(type).m_psz);
		auto it = std::find_if(types.begin(), types.end(), [&](auto& t) { return t->TypeName == type; });
		CHECK(it != types.end());
	}
}

TEST_CASE("Kernel structures are in the kernel's symbols", "[KernelTypes][symbols]") {
	// DIA is a COM object
	REQUIRE(SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)));
	auto& symbols = SymbolManager::Get();
	if (!symbols)
		SKIP("the kernel's symbols aren't available (needs DIA and _NT_SYMBOL_PATH)");

	for (auto& [type, structure] : ObjectHelpers::KernelTypes) {
		INFO("type " << CW2A(type).m_psz << ", structure " << CW2A(structure).m_psz);
		CHECK(symbols.GetSymbol(structure));
	}
}
