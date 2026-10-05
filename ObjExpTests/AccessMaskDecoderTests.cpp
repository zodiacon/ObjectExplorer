#include "pch.h"
#include <catch2/catch_test_macros.hpp>
#include "AccessMaskDecoder.h"

TEST_CASE("All access is decoded as a single name", "[AccessMaskDecoder]") {
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Process", PROCESS_ALL_ACCESS) == L"PROCESS_ALL_ACCESS");
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Event", EVENT_ALL_ACCESS) == L"EVENT_ALL_ACCESS");
}

TEST_CASE("Specific and generic rights are listed", "[AccessMaskDecoder]") {
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Event", EVENT_MODIFY_STATE | SYNCHRONIZE) == L"MODIFY_STATE | SYNCHRONIZE");

	auto text = AccessMaskDecoder::DecodeAccessMask(L"Process", PROCESS_VM_READ | PROCESS_DUP_HANDLE);
	CHECK(text.Find(L"VM_READ") >= 0);
	CHECK(text.Find(L"DUP_HANDLE") >= 0);
	CHECK(text.Find(L"PROCESS_ALL_ACCESS") < 0);
}

TEST_CASE("Generic rights alone", "[AccessMaskDecoder]") {
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Event", READ_CONTROL) == L"READ_CONTROL");
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Event", SYNCHRONIZE | WRITE_DAC) == L"SYNCHRONIZE | WRITE_DAC");
}

TEST_CASE("No access decodes to nothing", "[AccessMaskDecoder]") {
	CHECK(AccessMaskDecoder::DecodeAccessMask(L"Process", 0).IsEmpty());
}

TEST_CASE("Specific rights of an unknown type", "[AccessMaskDecoder]") {
	auto text = AccessMaskDecoder::DecodeAccessMask(L"NoSuchType", 0x0001 | SYNCHRONIZE);
	CHECK(text.Left(9) == L"<unknown>");
	CHECK(text.Find(L"SYNCHRONIZE") >= 0);
}
