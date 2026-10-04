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
	void SetTabIcon(IView* view, ViewIconType iconType);
	static void SetViewFont(HWND hView, HFONT font);

private:
	ViewFactory() = default;
	IView* AddView(IView* view, int image);

	IMainFrame* m_pFrame{ nullptr };
	CNativeCustomTabView* m_tabs;
	std::unordered_map<ViewIconType, int> m_tabIcons;
};

