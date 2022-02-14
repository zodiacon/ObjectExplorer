#include "pch.h"
#include "ObjectHelpers.h"
#include "GenericObjectProperties.h"
#include "ObjectPropertiesDlg.h"

UINT ObjectHelpers::ShowObjectProperties(HANDLE hObject, PCWSTR typeName, PCWSTR name) {
    CString title = CString(typeName) + L" Properties" + CString(name ? (L" (" + CString(name) + L")") : CString());
    CObjectPropertiesDlg dlg((PCWSTR)title, typeName);
    CGenericPropertiesPage page1(hObject, typeName, name);
    page1.Create(::GetActiveWindow());
    dlg.AddPage(L"General", page1);
    dlg.DoModal();

    return 0;
}
