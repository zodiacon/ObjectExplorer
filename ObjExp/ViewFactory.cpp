#include "pch.h"
#include "resource.h"
#include "ViewFactory.h"
#include "ObjectTypesView.h"
#include "ObjectManagerView.h"
#include "HandlesView.h"
#include "ObjectsView.h"
#include "ZombieProcessesView.h"

ViewFactory& ViewFactory::Get() {
    static ViewFactory factory;
    return factory;
}

bool ViewFactory::Init(IMainFrame* frame, CCustomTabView& tabs) {
    m_pFrame = frame;
    m_tabs = &tabs;
    UINT icons[] = {
        IDI_TYPES, IDI_PACKAGE, IDI_MAGNET, IDI_MAGNET2, IDI_OBJECTS,
        IDI_PROCESS_ZOMBIE
    };
    CImageList images;
    images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
    for (auto icon : icons)
        images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
    tabs.SetImageList(images);

    ViewIconType iconTypes[] = {
        ViewIconType::ZombieProcess,
    };
    for (auto icon : iconTypes) {
        int n = images.AddIcon(AtlLoadIconImage((UINT)icon, 0, 16, 16));
        m_tabIcons.insert({ icon, n });
    }

    return true;
}

IView* ViewFactory::CreateView(ViewType type, DWORD pid, PCWSTR sparam) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    IView* view{ nullptr };
    int image = -1;
    switch (type) {
        case ViewType::ObjectTypes:
        {
            auto p = new CObjectTypesView(m_pFrame);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style, 0);
            image = 0;
            view = p;
            break;
        }

        case ViewType::ObjectManager:
        {
            auto p = new CObjectManagerView(m_pFrame);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style, 0);
            image = 1;
            view = p;
            break;
        }

        case ViewType::ZombieProcesses:
        {
            auto p = new CZombieProcessesView(m_pFrame);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style, 0);
            image = 5;
            view = p;
            break;
        }

        case ViewType::AllHandles:
        case ViewType::ProcessHandles:
        case ViewType::HandlesOfType:
        {
            auto p = new CHandlesView(m_pFrame, pid, sparam);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style);
            image = type == ViewType::AllHandles ? 2 : 3;
            view = p;
            break;
        }
        case ViewType::Objects:
        {
            auto p = new CObjectsView(m_pFrame, sparam);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style);
            image = 4;
            view = p;
            break;
        }
    }
    if(view)
        m_tabs->AddPage(view->GetHwnd(), view->GetTitle(), image, view);

    return view;
}

void ViewFactory::SetTabIcon(IView* view, ViewIconType iconType) {
    int count = m_tabs->GetPageCount();
    int i;
    for (i = 0; i < count; i++) {
        if (m_tabs->GetPageData(i) == view) {
            m_tabs->SetPageImage(i, m_tabIcons[iconType]);
            break;
        }
    }
    ATLASSERT(i < count);
}
