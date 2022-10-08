#include "pch.h"
#include "StructurePage.h"
#include "DiaHelper.h"
#include "SymbolToTreeView.h"
#include "TreeListView.h"

CStructPage::CStructPage(HANDLE hObject, DiaSymbol const& sym) : m_hObject(hObject), m_Object(sym) {
}

LRESULT CStructPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout();

    m_Tree.SubclassWindow(GetDlgItem(IDC_TREE));
    auto header = m_Tree.GetHeaderControl();
    HDITEM col = { 0 };
    col.mask = HDI_FORMAT | HDI_TEXT | HDI_WIDTH;
    col.fmt = HDF_LEFT;
    col.cxy = 150;
    col.pszText = (PWSTR)_T("Member");
    header.InsertItem(0, &col);
    col.cxy = 150;
    col.pszText = (PWSTR)_T("Type");
    header.InsertItem(1, &col);
    col.cxy = 150;
    col.pszText = (PWSTR)_T("Value");
    header.InsertItem(2, &col);

    SymbolToTreeView::FillTreeView(m_Tree, TVI_ROOT, m_Object);

    return 0;
}
