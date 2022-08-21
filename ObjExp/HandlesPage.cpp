#include "pch.h"
#include "HandlesPage.h"
#include "AccessMaskDecoder.h"
#include "ProcessHelper.h"

CString CHandlesPage::GetColumnText(HWND, int row, int col) const {
    auto& hi = m_Handles[row];
    switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
        case ColumnType::Handle: return std::format(L"{} (0x{:X})", hi.HandleValue, hi.HandleValue).c_str();
        case ColumnType::PID: return std::format(L"{} (0x{:X})", hi.ProcessId, hi.ProcessId).c_str();
        case ColumnType::Attributes: return std::format(L"0x{:02X}", hi.HandleAttributes).c_str();
        case ColumnType::Access: return std::format(L"0x{:08X}", hi.GrantedAccess).c_str();
        case ColumnType::ProcessName: return ProcessHelper::GetProcessName(hi.ProcessId);
        case ColumnType::DecodedAccess: return AccessMaskDecoder::DecodeAccessMask(m_TypeName, hi.GrantedAccess);
    }
    return CString();
}

LRESULT CHandlesPage::OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
    InitDynamicLayout(false);
    PVOID address{ nullptr };
    m_Handles.clear();
    auto handles = ObjectManager::EnumHandles2<>(m_TypeName);
    for (auto& handle : handles) {
        if (handle->HandleValue == HandleToULong(m_hObject) && handle->ProcessId == ::GetCurrentProcessId()) {
            address = handle->Object;
            break;
        }
    }

    for (auto& handle : handles) {
        if (handle->Object == address && handle->HandleValue != HandleToULong(m_hObject)) {
            m_Handles.push_back(*handle);
        }
    }

    m_List.Attach(GetDlgItem(IDC_LIST));
    m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_FULLROWSELECT);
    auto cm = GetColumnManager(m_List);
    cm->AddColumn(L"Process Name", LVCFMT_LEFT, 140, ColumnType::ProcessName);
    cm->AddColumn(L"PID", LVCFMT_RIGHT, 100, ColumnType::PID);
    cm->AddColumn(L"Handle", LVCFMT_RIGHT, 100, ColumnType::Handle);
    cm->AddColumn(L"Access", LVCFMT_RIGHT, 100, ColumnType::Access);
    cm->AddColumn(L"Attributes", LVCFMT_RIGHT, 100, ColumnType::Attributes);
    cm->AddColumn(L"Decoded Access", LVCFMT_LEFT, 200, ColumnType::DecodedAccess);

    m_List.SetItemCount((int)m_Handles.size());

    return 0;
}

LRESULT CHandlesPage::OnDialogColor(UINT, WPARAM, LPARAM, BOOL&) {
    return (LRESULT)::GetSysColorBrush(COLOR_WINDOW);
}
