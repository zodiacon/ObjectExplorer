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
	Search,
	ZombieProcesses,
	ZombieThreads,
};

enum class ViewIconType {
	ZombieProcess = IDI_PROCESS_ZOMBIE,
};

struct ViewFactory final {
	static ViewFactory& Get();
	
	bool Init(IMainFrame* frame, CNativeCustomTabView& tabs);
	IView* CreateView(ViewType type, DWORD pid = 0, PCWSTR sparam = nullptr);
	void SetTabIcon(IView* view, ViewIconType iconType);
	static void SetViewFont(HWND hView, HFONT font);

private:
	ViewFactory() = default;

	IMainFrame* m_pFrame{ nullptr };
	CNativeCustomTabView* m_tabs;
	std::unordered_map<ViewIconType, int> m_tabIcons;
};

