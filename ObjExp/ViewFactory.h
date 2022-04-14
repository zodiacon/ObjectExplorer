#pragma once

#include "ObjectTypesView.h"
#include "Interfaces.h"
#include <type_traits>

enum class ViewType {
	ObjectTypes,
	ObjectManager,
	AllObjects,
	AllHandles,
	HandlesOfType,
	ProcessHandles,
	Search,
};

struct ViewFactory final {
	static ViewFactory& Get();
	
	bool Init(IMainFrame* frame, CTabView& tabs);
	IView* CreateView(ViewType type, DWORD pid = 0, PCWSTR sparam = nullptr);

private:
	ViewFactory() = default;

	IMainFrame* m_pFrame{ nullptr };
	CTabView* m_tabs;
};

