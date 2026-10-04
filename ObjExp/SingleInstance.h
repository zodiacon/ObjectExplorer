#pragma once

//
// the running instance publishes its main window in a named shared memory block,
// so another instance can activate it instead of starting
//
struct SingleInstance abstract final {
	// true if another instance's window was found and activated
	static bool ActivateOther();
	static void Register(HWND hWnd);
	static void Unregister(HWND hWnd);

private:
	static HWND* GetSharedWindow(bool create);

	inline static HANDLE s_hMapping;
	inline static HWND* s_pWindow;
};
