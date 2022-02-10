#include "pch.h"
#include "ViewFactory.h"
#include "ObjectTypesView.h"
#include "ObjectManagerView.h"
#include "resource.h"

void ViewFactory::InitIcons(CTabView& tabs) {
    UINT icons[] = {
        IDI_TYPES, IDI_PACKAGE,
    };
    CImageList images;
    images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
    for (auto icon : icons)
        images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
    tabs.SetImageList(images);
}

IView* ViewFactory::CreateView(IMainFrame* frame, CTabView& tabs, ViewType type, DWORD style) {
    IView* view{ nullptr };
    int image = -1;
    switch (type) {
        case ViewType::ObjectTypes:
        {
            auto p = new CObjectTypesView(frame);
            p->Create(tabs, CWindow::rcDefault, nullptr, style, 0);
            image = 0;
            view = p;
            break;
        }

        case ViewType::ObjectManager:
        {
            auto p = new CObjectManagerView(frame);
            p->Create(tabs, CWindow::rcDefault, nullptr, style, 0);
            image = 1;
            view = p;
            break;
        }
    }
    if(view)
        tabs.AddPage(view->GetHwnd(), view->GetTitle(), image, view);

    return view;
}
