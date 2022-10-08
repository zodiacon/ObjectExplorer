#include "pch.h"
#include "ObjectPropertiesDlg.h"
#include "ResourceManager.h"
#include "AppSettings.h"

bool CObjectPropertiesDlg::AddPage(PCWSTR title, HWND hPage) {
    TabItem item;
    item.Title = title;
    item.win.Attach(hPage);
    m_Pages.push_back(item);
    return true;
}

bool CObjectPropertiesDlg::AddPage(PCWSTR title, HPROPSHEETPAGE hPage) {
    TabItem item;
    item.Title = title;
    item.page.Attach((HWND)hPage);
    m_Pages.push_back(item);
    return true;
}

LRESULT CObjectPropertiesDlg::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();
    SetDialogIcon(ResourceManager::Get().GetTypeIcon(m_Type));
    SetWindowText(m_Title);
    m_Tabs.Attach(GetDlgItem(IDC_TABS));
    CImageList images;
    images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 4, 2);
    UINT icons[] = {
        IDI_INFO, IDI_MAGNET, IDI_STRUCT,
    };
    for(auto icon : icons)
        images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));
    m_Tabs.SetImageList(images);

    for(int i = 0; i < m_Pages.size(); i++) {
        auto& page = m_Pages[i];
        m_Tabs.AddItem(TCIF_TEXT | TCIF_IMAGE, page.Title, i, 0);
        page.win.SetParent(m_hWnd);
    }
    m_Pages[0].win.ShowWindow(SW_SHOW);
    m_Tabs.SetCurSel(m_SelectedPage = 0);
    AppSettings::Get().LoadWindowPosition(m_hWnd, L"ObjectPropertiesDialog");

    UpdateSize();
    return 0;
}

LRESULT CObjectPropertiesDlg::OnTabChanged(int, LPNMHDR, BOOL&) {
    int newPage = m_Tabs.GetCurSel();
    if (m_SelectedPage >= 0) {
        auto& win = m_Pages[m_SelectedPage].win;
        win.ShowWindow(SW_HIDE);
    }
    auto& win = m_Pages[m_SelectedPage = newPage].win;
    win.ShowWindow(SW_SHOW);
    ::SetParent(win.m_hWnd, m_hWnd);
    win.UpdateWindow();

    return 0;
}

LRESULT CObjectPropertiesDlg::OnOKCancel(WORD, WORD id, HWND, BOOL&) {
    AppSettings::Get().SaveWindowPosition(m_hWnd, L"ObjectPropertiesDialog");
    EndDialog(id);
    return 0;
}

LRESULT CObjectPropertiesDlg::OnSize(UINT code, WPARAM wp, LPARAM lp, BOOL& handled) {
    handled = FALSE;
    UpdateSize();

    return 0;
}

void CObjectPropertiesDlg::UpdateSize() {
    if (m_SelectedPage >= 0) {
        static int tabHeight = 0;
        if (tabHeight == 0) {
            CTabView view;
            view.m_tab.Attach(m_Tabs);
            tabHeight = view.CalcTabHeight();
        }
        CRect rc;
        m_Tabs.GetWindowRect(&rc);
        ScreenToClient(&rc);
        rc.top += tabHeight;
        rc.DeflateRect(2, 0);
        rc.bottom -= 6;
        for (auto& page : m_Pages)
            page.win.MoveWindow(&rc);
    }
}


