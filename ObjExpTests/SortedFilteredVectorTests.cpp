#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include <SortedFilteredVector.h>

namespace {
	SortedFilteredVector<int> OneToTen() {
		SortedFilteredVector<int> v;
		for (int i = 1; i <= 10; i++)
			v.push_back(i);
		return v;
	}

	std::vector<int> Shown(SortedFilteredVector<int> const& v) {
		std::vector<int> items;
		for (size_t i = 0; i < v.size(); i++)
			items.push_back(v[i]);
		return items;
	}
}

TEST_CASE("Items are shown in the order they're added", "[SortedFilteredVector]") {
	auto v = OneToTen();
	CHECK(v.size() == 10);
	CHECK(v.TotalSize() == 10);
	CHECK(Shown(v) == std::vector{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
}

TEST_CASE("A filter hides items, and applies to items added later", "[SortedFilteredVector]") {
	auto v = OneToTen();
	v.Filter([](int value, size_t) { return value % 2 == 0; });
	CHECK(Shown(v) == std::vector{ 2, 4, 6, 8, 10 });
	CHECK(v.TotalSize() == 10);

	v.push_back(12);
	v.push_back(13);
	CHECK(Shown(v) == std::vector{ 2, 4, 6, 8, 10, 12 });
	CHECK(v.TotalSize() == 12);

	v.Filter(nullptr);
	CHECK(v.size() == 12);
	CHECK(v[11] == 13);
}

TEST_CASE("Sorting reorders the shown items only", "[SortedFilteredVector]") {
	auto v = OneToTen();
	v.Sort([](int a, int b) { return a > b; });
	CHECK(Shown(v) == std::vector{ 10, 9, 8, 7, 6, 5, 4, 3, 2, 1 });
	CHECK(v.GetReal(0) == 1);	// the items themselves stay in place

	v.ClearSort();
	CHECK(Shown(v) == std::vector{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
}

TEST_CASE("Removing by shown index removes the right item", "[SortedFilteredVector]") {
	auto v = OneToTen();
	v.Filter([](int value, size_t) { return value > 5; });
	v.Sort([](int a, int b) { return a > b; });
	REQUIRE(Shown(v) == std::vector{ 10, 9, 8, 7, 6 });

	v.Remove(1);	// 9
	CHECK(Shown(v) == std::vector{ 10, 8, 7, 6 });
	CHECK(v.TotalSize() == 9);

	auto& all = v.GetRealAll();
	CHECK(std::find(all.begin(), all.end(), 9) == all.end());

	v.Filter(nullptr);
	CHECK(Shown(v) == std::vector{ 1, 2, 3, 4, 5, 6, 7, 8, 10 });
}

TEST_CASE("Erasing past the end does nothing", "[SortedFilteredVector]") {
	auto v = OneToTen();
	CHECK_FALSE(v.erase(10));
	CHECK(v.erase(0));
	CHECK(v.size() == 9);
	CHECK(v[0] == 2);
}
