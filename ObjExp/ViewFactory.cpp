#include "pch.h"
#include "ViewFactory.h"
#include "ObjectTypesView.h"
#include "resource.h"

void ViewFactory::InitIcons(CTabView& tabs) {
    UINT icons[] = {
        IDI_TYPES,
    };
    CImageList images;
    images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
    for (auto icon : icons)
        images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
    tabs.SetImageList(images);
}

IView* ViewFactory::CreateView(IMainFrame* frame, HWND hParent, ViewType type, DWORD style) {
    IView* view{ nullptr };
    switch (type) {
        case ViewType::ObjectTypes:
            auto p = new CObjectTypesView(frame);
            p->Create(hParent, CWindow::rcDefault, nullptr, style, 0);
            view = p;
            break;
    }
    return view;
}
