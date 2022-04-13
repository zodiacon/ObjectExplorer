#pragma once

#include "ObjectTypesView.h"
#include "Interfaces.h"
#include <type_traits>

enum class ViewType {
	ObjectTypes,
	ObjectManager,
	AllObjects,
	AllHandles,
	ProcessHandles,
	Search,
};

struct ViewFactory abstract final {
	static void InitIcons(CTabView& tabs);
	static IView* CreateView(IMainFrame* frame, CTabView& tabs, ViewType type, DWORD pid = 0);
};

