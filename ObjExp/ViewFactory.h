#pragma once

#include "ObjectTypesView.h"
#include "Interfaces.h"

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
	static IView* CreateView(IMainFrame* frame, CTabView& tabs, ViewType type, 
		DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
};

