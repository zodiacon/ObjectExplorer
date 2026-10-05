#include "pch.h"
#include "resource.h"
#include "ViewFactory.h"
#include "ObjectTypesView.h"
#include "ObjectManagerView.h"
#include "HandlesView.h"
#include "ObjectsView.h"
#include "ZombieProcessesView.h"
#include "SearchView.h"
#include "PipesView.h"
#include "ProcessesView.h"
#include "SystemInfoView.h"
#include "ResourceManager.h"
#include "SecurityHelper.h"

ViewFactory& ViewFactory::Get() {
    static ViewFactory factory;
    return factory;
}

bool ViewFactory::Init(IMainFrame* frame, CNativeCustomTabView& tabs) {
    m_pFrame = frame;
    m_tabs = &tabs;
    // tab images, by index
    UINT icons[] = {
        IDI_TYPES, IDI_PACKAGE, IDI_MAGNET, IDI_MAGNET2, IDI_OBJECTS,
        IDI_PROCESS_ZOMBIE, IDI_THREAD_ZOMBIE, IDI_FIND, IDI_PLUG, IDI_MESSAGE,
        IDI_PROCESS, IDI_THREAD, IDI_INFO,
    };
    // lives as long as the tab view
    CImageList images;
    images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 4);
    for (auto icon : icons)
        ResourceManager::AddIcon(images, icon);
    tabs.SetImageList(images);

    ViewIconType iconTypes[] = {
        ViewIconType::ZombieProcess,
    };
    for (auto icon : iconTypes) {
        int n = ResourceManager::AddIcon(images, (UINT)icon);
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
        case ViewType::ZombieThreads:
        {
            auto p = new CZombieProcessesView(m_pFrame, type ==  ViewType::ZombieProcesses);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style, 0);
            image = type == ViewType::ZombieProcesses ? 5 : 6;
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

        case ViewType::Search:
            return CreateSearchView(sparam, false);

        case ViewType::Pipes:
        case ViewType::Mailslots:
        {
            auto p = new CPipesView(m_pFrame, type == ViewType::Mailslots);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style);
            image = type == ViewType::Pipes ? 8 : 9;
            view = p;
            break;
        }

        case ViewType::Processes:
        case ViewType::Threads:
        {
            auto p = new CProcessesView(m_pFrame, type == ViewType::Threads);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style);
            image = type == ViewType::Processes ? 10 : 11;
            view = p;
            break;
        }

        case ViewType::SystemInformation:
        {
            auto p = new CSystemInfoView(m_pFrame);
            p->Create(*m_tabs, CWindow::rcDefault, nullptr, style);
            image = 12;
            view = p;
            break;
        }
    }
    return AddView(view, image, { type, sparam, pid });
}

IView* ViewFactory::CreateSearchView(PCWSTR text, bool matchCase) {
    auto p = new CSearchView(m_pFrame, text, matchCase);
    p->Create(*m_tabs, CWindow::rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
    return AddView(p, 7, { ViewType::Search, text, 0, matchCase });
}

IView* ViewFactory::DuplicateView(IView* view) {
    auto it = m_Views.find(view);
    if (it == m_Views.end())
        return nullptr;
    // a copy: creating the view adds to the map
    auto info = it->second;
    if (info.Type == ViewType::Search)
        return CreateSearchView(info.Param, info.MatchCase);
    return CreateView(info.Type, info.Pid, info.Param.IsEmpty() ? nullptr : (PCWSTR)info.Param);
}

IView* ViewFactory::AddView(IView* view, int image, ViewInfo info) {
    if (view) {
        if (auto font = m_pFrame->GetViewFont())
            SetViewFont(view->GetHwnd(), font);
        m_Views[view] = std::move(info);
        m_tabs->AddPage(view->GetHwnd(), view->GetTitle(), image, view);
    }
    return view;
}

namespace {
    // the views that are restored, by the names they're saved with
    const std::pair<ViewType, PCWSTR> RestorableViews[] = {
        { ViewType::ObjectTypes, L"ObjectTypes" },
        { ViewType::ObjectManager, L"ObjectManager" },
        { ViewType::AllHandles, L"AllHandles" },
        { ViewType::HandlesOfType, L"HandlesOfType" },
        { ViewType::Objects, L"Objects" },
        { ViewType::ZombieProcesses, L"ZombieProcesses" },
        { ViewType::ZombieThreads, L"ZombieThreads" },
        { ViewType::Pipes, L"Pipes" },
        { ViewType::Mailslots, L"Mailslots" },
        { ViewType::Processes, L"Processes" },
        { ViewType::Threads, L"Threads" },
        { ViewType::SystemInformation, L"SystemInformation" },
    };
}

std::vector<std::wstring> ViewFactory::SaveViews(int& activeIndex) const {
    std::vector<std::wstring> views;
    activeIndex = -1;
    int active = m_tabs->GetActivePage();
    for (int i = 0; i < m_tabs->GetPageCount(); i++) {
        auto it = m_Views.find((IView*)m_tabs->GetPageData(i));
        if (it == m_Views.end())
            continue;
        auto& type = it->second.Type;
        auto& param = it->second.Param;
        auto name = std::find_if(std::begin(RestorableViews), std::end(RestorableViews), [&](auto& v) { return v.first == type; });
        if (name == std::end(RestorableViews))
            continue;
        if (i == active)
            activeIndex = (int)views.size();
        // the format is name|parameter
        views.push_back(std::wstring(name->second) + L"|" + (PCWSTR)param);
    }
    return views;
}

int ViewFactory::RestoreViews(std::vector<std::wstring> const& views, int activeIndex) {
    int activePage = -1;
    for (int i = 0; i < (int)views.size(); i++) {
        auto& view = views[i];
        auto sep = view.find(L'|');
        auto name = view.substr(0, sep);
        auto param = sep == std::wstring::npos ? std::wstring() : view.substr(sep + 1);
        auto it = std::find_if(std::begin(RestorableViews), std::end(RestorableViews), [&](auto& v) { return name == v.second; });
        if (it == std::end(RestorableViews))
            continue;
        // all objects requires running elevated
        if (it->first == ViewType::Objects && param.empty() && !SecurityHelper::IsRunningElevated())
            continue;
        if (CreateView(it->first, 0, param.empty() ? nullptr : param.c_str()) && i == activeIndex)
            activePage = m_tabs->GetPageCount() - 1;
    }
    return activePage;
}

void ViewFactory::SetViewFont(HWND hView, HFONT font) {
    //
    // only the data controls (lists and trees) get the font; toolbars, edits etc. keep theirs
    //
    ::EnumChildWindows(hView, [](HWND hWnd, LPARAM font) -> BOOL {
        WCHAR className[32];
        if (::GetClassName(hWnd, className, _countof(className)) &&
            (::_wcsicmp(className, WC_LISTVIEW) == 0 || ::_wcsicmp(className, WC_TREEVIEW) == 0))
            ::SendMessage(hWnd, WM_SETFONT, (WPARAM)font, TRUE);
        return TRUE;
        }, (LPARAM)font);
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
