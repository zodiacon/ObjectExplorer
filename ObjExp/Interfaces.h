#pragma once

struct IMainFrame abstract {
	virtual HWND GetHwnd() const = 0;
	virtual BOOL TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) = 0;
	virtual CUpdateUIBase& GetUI() = 0;
};

struct IView {
	virtual void PageActivated(bool active) {}
	virtual bool ProcessCommand(UINT cmd) {
		return false;
	}
	virtual HWND GetHwnd() const = 0;
	virtual CString GetTitle() const = 0;
};
