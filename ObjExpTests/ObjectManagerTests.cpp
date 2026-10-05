#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "ObjectManager.h"

namespace {
	void EnsureTypes() {
		if (ObjectManager::GetObjectTypes().empty())
			ObjectManager::EnumTypes();
	}

	USHORT TypeIndex(PCWSTR name) {
		EnsureTypes();
		return ObjectManager::GetType(name)->TypeIndex;
	}
}

TEST_CASE("Object types are enumerated", "[ObjectManager][EnumTypes]") {
	auto count = ObjectManager::EnumTypes();
	auto& types = ObjectManager::GetObjectTypes();
	CHECK(count >= 40);
	CHECK((size_t)count == types.size());

	for (auto name : { L"Process", L"Thread", L"File", L"Event", L"Directory", L"SymbolicLink", L"Key" }) {
		INFO("type " << CW2A(name).m_psz);
		auto it = std::find_if(types.begin(), types.end(), [&](auto& type) { return type->TypeName == name; });
		CHECK(it != types.end());
	}
	CHECK(ObjectManager::TotalHandles > 0);
	CHECK(ObjectManager::TotalObjects > 0);
	CHECK(ObjectManager::PeakHandles >= ObjectManager::TotalHandles);
}

TEST_CASE("Types are found by index and by name", "[ObjectManager][EnumTypes]") {
	EnsureTypes();
	for (auto& type : ObjectManager::GetObjectTypes()) {
		CHECK(ObjectManager::GetType(type->TypeIndex) == type);
		CHECK(ObjectManager::GetType(type->TypeName) == type);
	}
}

TEST_CASE("Enumerating the types again updates the same objects", "[ObjectManager][EnumTypes]") {
	EnsureTypes();
	auto before = ObjectManager::GetObjectTypes();	// a copy of the pointers
	auto count = ObjectManager::EnumTypes();
	auto& after = ObjectManager::GetObjectTypes();
	REQUIRE((size_t)count >= before.size());
	for (size_t i = 0; i < before.size(); i++)
		CHECK(after[i] == before[i]);
}

TEST_CASE("An unknown type index gets a placeholder", "[ObjectManager][EnumTypes]") {
	EnsureTypes();
	auto type = ObjectManager::GetType(250);
	REQUIRE(type);
	CHECK(type->TypeName == L"<Type 250>");
}

TEST_CASE("A handle of this process is in the handle list", "[ObjectManager]") {
	wil::unique_event_nothrow event;
	REQUIRE(SUCCEEDED(event.create()));
	auto handles = ObjectManager::EnumHandles2<>(L"Event", ::GetCurrentProcessId(), false, true);
	auto it = std::find_if(handles.begin(), handles.end(), [&](auto& hi) { return hi->HandleValue == HandleToULong(event.get()); });
	REQUIRE(it != handles.end());
	CHECK((*it)->ObjectTypeIndex == TypeIndex(L"Event"));
	CHECK((*it)->ProcessId == ::GetCurrentProcessId());
}

TEST_CASE("Object names", "[ObjectManager]") {
	EnsureTypes();
	auto pid = ::GetCurrentProcessId();

	SECTION("a named event") {
		auto name = std::format(L"ObjExpTests_{}", pid);
		wil::unique_event_nothrow event(::CreateEvent(nullptr, TRUE, FALSE, name.c_str()));
		REQUIRE(event);
		auto objectName = ObjectManager::GetObjectName(event.get(), TypeIndex(L"Event"));
		CHECK(objectName.Right((int)name.size()) == name.c_str());
		CHECK(objectName.Find(L"BaseNamedObjects") >= 0);

		// the same, through another process' view: duplicating the handle by process ID
		CHECK(ObjectManager::GetObjectName(event.get(), pid, TypeIndex(L"Event")) == objectName);
	}

	SECTION("a file (named on a worker thread, with a timeout)") {
		WCHAR path[MAX_PATH];
		::GetModuleFileName(nullptr, path, _countof(path));
		wil::unique_hfile file(::CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
		REQUIRE(file);
		auto objectName = ObjectManager::GetObjectName(file.get(), TypeIndex(L"File"));
		CString fileName = wcsrchr(path, L'\\');
		CHECK(objectName.Left(8) == L"\\Device\\");
		CHECK(objectName.Right(fileName.GetLength()).CompareNoCase(fileName) == 0);
	}

	SECTION("processes and threads have no names") {
		CHECK(ObjectManager::GetObjectName(::GetCurrentProcess(), TypeIndex(L"Process")).IsEmpty());
	}
}

TEST_CASE("Objects in the namespace", "[ObjectManager]") {
	auto root = ObjectManager::EnumDirectoryObjects(L"\\");
	CHECK(!root.empty());
	auto it = std::find_if(root.begin(), root.end(), [](auto& item) { return item.Name == L"BaseNamedObjects"; });
	CHECK(it != root.end());

	// \GLOBAL??\C: (or whatever the system drive is) is a link to its volume device
	WCHAR system[MAX_PATH];
	::GetSystemDirectory(system, _countof(system));
	auto target = ObjectManager::GetSymbolicLinkTarget(CString(L"\\GLOBAL??\\") + CString(system, 2));
	CHECK(target.Left(8) == L"\\Device\\");
}
