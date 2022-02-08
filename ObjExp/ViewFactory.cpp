#include "pch.h"
#include "ViewFactory.h"
#include "ObjectTypesView.h"

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
