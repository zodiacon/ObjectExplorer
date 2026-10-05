#pragma once

#include "ObjectTypesView.h"
#include "Interfaces.h"
#include <type_traits>
#include <NativeCustomTabView.h>

enum class ViewType {
	ObjectTypes,
	ObjectManager,
	AllObjects,
	AllHandles,
	HandlesOfType,
	ProcessHandles,
	Objects,
	Search,		// use CreateSearchView
	ZombieProcesses,
	ZombieThreads,
	Pipes,
	Mailslots,
	Processes,
	Threads,
	SystemInformation,
};

enum class ViewIconType {
	ZombieProcess = IDI_PROCESS_ZOMBIE,
};

struct ViewFactory final {
	static ViewFactory& Get();
	
	bool Init(IMainFrame* frame, CNativeCustomTabView& tabs);
	IView* CreateView(ViewType type, DWORD pid = 0, PCWSTR sparam = nullptr);
	// searches the names of all handles and namespace objects for the text
	IView* CreateSearchView(PCWSTR text, bool matchCase);

	//
	// open tabs, kept across runs; process specific and search tabs aren't restored
	// (a process ID may be another process by then, and searches can take a while)
	//
	std::vector<std::wstring> SaveViews(int& activeIndex) const;
	// returns the page to activate, or -1
	int RestoreViews(std::vector<std::wstring> const& views, int activeIndex);

	// a new view like the given one (same kind and parameters)
	IView* DuplicateView(IView* view);
	void SetTabIcon(IView* view, ViewIconType iconType);
	static void SetViewFont(HWND hView, HFONT font);

private:
	ViewFactory() = default;

	struct ViewInfo {
		ViewType Type;
		CString Param;
		DWORD Pid{ 0 };
		bool MatchCase{ false };	// search views
	};
	IView* AddView(IView* view, int image, ViewInfo info);

	// how each view was created; entries of closed views stay, but only open ones are looked up
	std::unordered_map<IView*, ViewInfo> m_Views;

	IMainFrame* m_pFrame{ nullptr };
	CNativeCustomTabView* m_tabs;
	std::unordered_map<ViewIconType, int> m_tabIcons;
};

