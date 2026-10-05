#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "SearchMatcher.h"

TEST_CASE("Search matching ignores case by default", "[SearchMatcher]") {
	SearchMatcher matcher(L"NamedPipe", false);
	CHECK(matcher.Matches(L"\\Device\\NamedPipe\\lsass"));
	CHECK(matcher.Matches(L"\\device\\namedpipe\\lsass"));
	CHECK(matcher.Matches(L"NAMEDPIPE"));
	CHECK_FALSE(matcher.Matches(L"\\Device\\Mailslot\\x"));
	CHECK_FALSE(matcher.Matches(L""));
}

TEST_CASE("Search matching with match case", "[SearchMatcher]") {
	SearchMatcher matcher(L"NamedPipe", true);
	CHECK(matcher.Matches(L"\\Device\\NamedPipe\\lsass"));
	CHECK_FALSE(matcher.Matches(L"\\device\\namedpipe\\lsass"));
}

TEST_CASE("Search matching finds a substring anywhere", "[SearchMatcher]") {
	SearchMatcher matcher(L"bno", false);
	CHECK(matcher.Matches(L"BNO"));
	CHECK(matcher.Matches(L"\\Sessions\\1\\BNOLINKS"));
	CHECK(matcher.Matches(L"prefix-bno-suffix"));
	CHECK_FALSE(matcher.Matches(L"bn"));
}
