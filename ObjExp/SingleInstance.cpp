#include "pch.h"
#include "SingleInstance.h"

static const WCHAR MappingName[] = L"ScorpioSoftware.ObjectExplorer.MainWindow";

HWND* SingleInstance::GetSharedWindow(bool create) {
	if (s_pWindow)
		return s_pWindow;

	// kept open for the life of the process, so the block exists as long as an instance runs
	s_hMapping = create
		? ::CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(HWND), MappingName)
		: ::OpenFileMapping(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, MappingName);
	if (!s_hMapping)
		return nullptr;

	s_pWindow = static_cast<HWND*>(::MapViewOfFile(s_hMapping, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, sizeof(HWND)));
	if (!s_pWindow) {
		::CloseHandle(s_hMapping);
		s_hMapping = nullptr;
	}
	return s_pWindow;
}

bool SingleInstance::ActivateOther() {
	auto pWindow = GetSharedWindow(false);
	if (!pWindow)
		return false;

	auto hWnd = *pWindow;
	if (!hWnd || !::IsWindow(hWnd))
		return false;

	if (::IsIconic(hWnd))
		::ShowWindow(hWnd, SW_RESTORE);
	::SetForegroundWindow(hWnd);
	return true;
}

void SingleInstance::Register(HWND hWnd) {
	if (auto pWindow = GetSharedWindow(true))
		*pWindow = hWnd;
}

void SingleInstance::Unregister(HWND hWnd) {
	// another instance may have registered since
	if (s_pWindow && *s_pWindow == hWnd)
		*s_pWindow = nullptr;
}
