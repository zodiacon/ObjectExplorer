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
	static IView* CreateView(IMainFrame* frame, HWND hParent, ViewType type, 
		DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
};

