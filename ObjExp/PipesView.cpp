#include "pch.h"
#include "PipesView.h"
#include "NtDll.h"
#include "ResourceManager.h"
#include "SortHelper.h"
#include "ListViewhelper.h"
#include "ClipboardHelper.h"
#include "ViewFactory.h"
#include "ObjectManager.h"
#include "ObjectHelpers.h"

namespace {
	typedef struct _FILE_DIRECTORY_INFORMATION {
		ULONG NextEntryOffset;
		ULONG FileIndex;
		LARGE_INTEGER CreationTime;
		LARGE_INTEGER LastAccessTime;
		LARGE_INTEGER LastWriteTime;
		LARGE_INTEGER ChangeTime;
		LARGE_INTEGER EndOfFile;		// pipes: current number of instances
		LARGE_INTEGER AllocationSize;	// pipes: maximum number of instances (0xFFFFFFFF for unlimited)
		ULONG FileAttributes;
		ULONG FileNameLength;
		WCHAR FileName[1];
	} FILE_DIRECTORY_INFORMATION;

	const int FileDirectoryInformation = 1;
	const NTSTATUS StatusNoMoreFiles = 0x80000006;
}

extern "C" NTSTATUS NTAPI NtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
	PIO_STATUS_BLOCK IoStatusBlock, PVOID FileInformation, ULONG Length, int FileInformationClass,
	BOOLEAN ReturnSingleEntry, PUNICODE_STRING FileName, BOOLEAN RestartScan);

CPipesView::CPipesView(IMainFrame* frame, bool mailslots) : CViewBase(frame), m_Mailslots(mailslots) {
}

CString CPipesView::GetTitle() const {
	return m_Mailslots ? L"Mailslots" : L"Pipes";
}

void CPipesView::Refresh() {
	CWaitCursor wait;
	m_Items.clear();

	wil::unique_hfile hDir(::CreateFile(m_Mailslots ? L"\\\\.\\mailslot\\" : L"\\\\.\\pipe\\", FILE_LIST_DIRECTORY | SYNCHRONIZE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr));
	if (hDir) {
		auto buffer = std::make_unique<BYTE[]>(1 << 16);
		IO_STATUS_BLOCK ioStatus;
		for (bool first = true; ; first = false) {
			auto status = NtQueryDirectoryFile(hDir.get(), nullptr, nullptr, nullptr, &ioStatus, buffer.get(), 1 << 16,
				FileDirectoryInformation, FALSE, nullptr, first);
			if (!NT_SUCCESS(status) || status == StatusNoMoreFiles)
				break;

			for (auto info = (FILE_DIRECTORY_INFORMATION*)buffer.get(); ; info = (FILE_DIRECTORY_INFORMATION*)((PBYTE)info + info->NextEntryOffset)) {
				Item item;
				item.Name = CString(info->FileName, info->FileNameLength / sizeof(WCHAR));
				item.Instances = info->EndOfFile.QuadPart;
				item.MaxInstances = info->AllocationSize.QuadPart;
				m_Items.push_back(std::move(item));
				if (info->NextEntryOffset == 0)
					break;
			}
		}
	}

	DoSort(GetSortInfo(m_List));
	m_List.SetItemCountEx((int)m_Items.size(), LVSICF_NOSCROLL);
	m_List.RedrawItems(m_List.GetTopIndex(), m_List.GetTopIndex() + m_List.GetCountPerPage());
	UpdateStatusText();
}

CString CPipesView::GetFullName(Item const& item) const {
	return (m_Mailslots ? L"\\Device\\Mailslot\\" : L"\\Device\\NamedPipe\\") + item.Name;
}

void CPipesView::DoSort(SortInfo const* si) {
	if (si == nullptr)
		return;

	auto col = GetColumnManager(m_List)->GetColumnTag<ColumnType>(si->SortColumn);
	auto asc = si->SortAscending;
	std::sort(m_Items.begin(), m_Items.end(), [&](auto const& i1, auto const& i2) {
		switch (col) {
			case ColumnType::Name:
			case ColumnType::FullName:
				return SortHelper::Sort(i1.Name, i2.Name, asc);
			case ColumnType::Instances: return SortHelper::Sort(i1.Instances, i2.Instances, asc);
			case ColumnType::MaxInstances: return SortHelper::Sort((ULONGLONG)i1.MaxInstances, (ULONGLONG)i2.MaxInstances, asc);
		}
		return false;
		});
}

CString CPipesView::GetColumnText(HWND, int row, int col) const {
	auto& item = m_Items[row];
	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Name: return item.Name;
		case ColumnType::FullName: return GetFullName(item);
		case ColumnType::Instances: return std::to_wstring(item.Instances).c_str();
		// unlimited is reported as -1 in 32 bits
		case ColumnType::MaxInstances: return (ULONG)item.MaxInstances == ULONG_MAX ? CString(L"Unlimited") : CString(std::to_wstring(item.MaxInstances).c_str());
	}
	return L"";
}

int CPipesView::GetRowImage(HWND, int row, int col) const {
	return ResourceManager::Get().GetTypeImage(L"File");
}

void CPipesView::UpdateUI(bool force) {
	auto& ui = UI();
	int selected = m_List.GetSelectedCount();
	ui.UIEnable(ID_VIEW_PROPERTIES, selected == 1);
	ui.UIEnable(ID_EDIT_COPY, selected > 0);
}

bool CPipesView::OnDoubleClickList(HWND, int row, int col, POINT const& pt) const {
	if (row >= 0)
		ShowProperties(row);
	return true;
}

bool CPipesView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	if (row < 0)
		return false;
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return GetFrame()->TrackPopupMenu(menu.GetSubMenu(5), 0, pt.x, pt.y);
}

void CPipesView::ShowProperties(int row) const {
	auto fullName = GetFullName(m_Items[row]);
	//
	// opening the pipe or mailslot by name would connect to it as a client (taking one of a pipe's instances),
	// which its server would see; use a handle that already exists instead (normally the server's)
	//
	HANDLE hObject = nullptr;
	{
		CWaitCursor wait;
		for (auto& hi : ObjectManager::EnumHandles2<>(L"File", 0, false, true)) {
			auto name = ObjectManager::GetObjectName(ULongToHandle(hi->HandleValue), hi->ProcessId, hi->ObjectTypeIndex, hi->Object);
			if (name.CompareNoCase(fullName) == 0) {
				hObject = ObjectManager::DupHandle(ULongToHandle(hi->HandleValue), hi->ProcessId);
				if (hObject)
					break;
			}
		}
	}
	if (!hObject) {
		AtlMessageBox(m_hWnd, L"No accessible handle to this object was found. Running elevated gives access to more processes' handles.",
			IDS_TITLE, MB_ICONINFORMATION);
		return;
	}
	ObjectHelpers::ShowObjectProperties(hObject, L"File", fullName);
	::CloseHandle(hObject);
}

void CPipesView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	UpdateUI();
}

void CPipesView::OnPageActivated(bool active) {
	if (active) {
		UpdateUI();
		UpdateStatusText();
	}
}

void CPipesView::FindHandles(int row) const {
	// handles to a pipe or mailslot are named by their full device path
	ViewFactory::Get().CreateSearchView(GetFullName(m_Items[row]), false);
}

void CPipesView::UpdateStatusText() const {
	if (IsActive())
		GetFrame()->SetStatusText(7, std::format(L"{}: {}", (PCWSTR)GetTitle(), m_Items.size()).c_str());
}

LRESULT CPipesView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, ListViewDefaultStyle);
	auto cm = GetColumnManager(m_List);

	cm->AddColumn(L"Name", LVCFMT_LEFT, 350, ColumnType::Name);
	if (!m_Mailslots) {
		cm->AddColumn(L"Instances", LVCFMT_RIGHT, 80, ColumnType::Instances, ColumnFlags::Visible | ColumnFlags::Numeric);
		cm->AddColumn(L"Max Instances", LVCFMT_RIGHT, 100, ColumnType::MaxInstances, ColumnFlags::Visible | ColumnFlags::Numeric);
	}
	cm->AddColumn(L"Full Name", LVCFMT_LEFT, 450, ColumnType::FullName);
	cm->UpdateColumns();

	m_List.SetExtendedListViewStyle(LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);
	m_List.SetImageList(ResourceManager::Get().GetTypesImageList(), LVSIL_SMALL);

	Refresh();
	return 0;
}

LRESULT CPipesView::OnEditCopy(WORD, WORD, HWND, BOOL&) const {
	ClipboardHelper::CopyText(m_hWnd, ListViewHelper::GetSelectedRowsAsString(m_List, L","));
	return 0;
}

LRESULT CPipesView::OnFindHandles(WORD, WORD, HWND, BOOL&) const {
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (row >= 0)
		FindHandles(row);
	return 0;
}

LRESULT CPipesView::OnViewProperties(WORD, WORD, HWND, BOOL&) const {
	int row = m_List.GetNextItem(-1, LVNI_SELECTED);
	if (row >= 0)
		ShowProperties(row);
	return 0;
}

LRESULT CPipesView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}
