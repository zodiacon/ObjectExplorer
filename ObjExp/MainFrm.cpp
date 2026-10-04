
#include "pch.h"
#include "resource.h"
#include "AboutDlg.h"
#include "ViewBase.h"
#include "MainFrm.h"
#include "SecurityHelper.h"
#include "IconHelper.h"
#include "ViewFactory.h"
#include <Psapi.h>
#include "ProcessSelectorDlg.h"
#include <VersionResourceHelper.h>
#include "AppSettings.h"
#include "SingleInstance.h"
#include "FindDlg.h"
#include <thread>

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::InitMenu(HMENU hMenu) {
	MenuItemData const commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_OBJECTS_OBJECTTYPES, IDI_TYPES },
		{ ID_OBJECTS_OBJECTMANAGERNAMESPACE, IDI_PACKAGE },
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
		{ ID_OPTIONS_ALWAYSONTOP, IDI_PIN },
		{ ID_OPTIONS_FONT, IDI_FONT },
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ ID_FILE_SAVE, IDI_SAVE },
		{ ID_VIEW_FIND, IDI_FIND },
		{ ID_VIEW_PROPERTIES, IDI_PROPERTIES },
		{ ID_OBJECTLIST_JUMPTOTARGET, IDI_TARGET },
		{ ID_VIEW_QUICKFIND, IDI_SEARCH },
		{ ID_OBJECTLIST_SHOWDIRECTORIESINLIST, IDI_DIRECTORY },
		{ ID_OBJECTLIST_LISTMODE, IDI_LIST },
		{ ID_OBJECTS_ALLHANDLES, IDI_MAGNET },
		{ ID_OBJECTS_ALLOBJECTS, IDI_OBJECTS },
		{ ID_SYSTEM_ZOMBIEPROCESSES, IDI_PROCESS_ZOMBIE },
		{ ID_SYSTEM_ZOMBIETHREADS, IDI_THREAD_ZOMBIE },
		{ ID_OBJECTS_PIPES, IDI_PLUG },
		{ ID_OBJECTS_MAILSLOTS, IDI_MESSAGE },
		{ ID_SYSTEM_PROCESSES, IDI_PROCESS },
		{ ID_SYSTEM_THREADS, IDI_THREAD },
		{ ID_SYSTEM_SYSTEMINFORMATION, IDI_INFO },
		{ ID_OBJECTS_HANDLESINPROCESS, IDI_MAGNET2 },
		{ ID_TYPESLIST_ALLHANDLES, IDI_MAGNET2 },
		{ ID_TYPESLIST_ALLOBJECTS, IDI_OBJECTS },
		{ ID_HANDLELIST_CLOSE, IDI_DELETE },
	};

	WTLHelper::InitMenu(hMenu, commands, _countof(commands));
}

HWND CMainFrame::GetHwnd() const {
	return m_hWnd;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
	InitMenu(hMenu);
	return ::TrackPopupMenu(hMenu, flags, x, y, 0, m_hWnd, nullptr);
}

CUpdateUIBase& CMainFrame::GetUI() {
	return *this;
}

bool CMainFrame::AddToolBar(HWND tb) {
	return UIAddToolBar(tb);
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	CreateSimpleStatusBar();
	m_StatusBar.SubclassWindow(m_hWndStatusBar);
	int parts[] = { 100, 200, 300, 430, 560, 750, 990, 1200, 1400 };
	m_StatusBar.SetParts(_countof(parts), parts);
	SetTimer(100, 2000);

	ToolBarButtonInfo const buttons[] = {
		{ ID_VIEW_REFRESH, IDI_REFRESH },
		{ 0 },
		{ ID_RUN, IDI_PLAY, BTNS_CHECK },
		{ 0 },
		{ ID_EDIT_COPY, IDI_COPY },
		{ 0 },
		{ ID_VIEW_FIND, IDI_FIND },
		{ 0 },
		{ ID_OBJECTS_OBJECTTYPES, IDI_TYPES },
		{ ID_OBJECTS_OBJECTMANAGERNAMESPACE, IDI_PACKAGE },
		{ ID_OBJECTS_ALLHANDLES, IDI_MAGNET, BTNS_BUTTON, L"All Handles" },
		{ ID_OBJECTS_ALLOBJECTS, IDI_OBJECTS, BTNS_BUTTON, L"All Objects" },
		{ ID_OBJECTS_HANDLESINPROCESS, IDI_MAGNET2, BTNS_BUTTON, L"Process Handles" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons));
	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);

	m_view.m_bTabCloseButton = FALSE;
	m_hWndClient = m_view.Create(m_hWnd, rcDefault, nullptr, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN, 0);
	ViewFactory::Get().Init(this, m_view);

	UISetCheck(ID_VIEW_STATUS_BAR, 1);

	CString text;
	GetWindowText(text);
	VersionResourceHelper vh;
	text += L" " + vh.GetValue(L"ProductVersion");
	SetWindowText(text);

	CMenuHandle hMenu = GetMenu();
	if (SecurityHelper::IsRunningElevated()) {
		hMenu.GetSubMenu(0).DeleteMenu(ID_FILE_RUNASADMINISTRATOR, MF_BYCOMMAND);
		hMenu.GetSubMenu(0).DeleteMenu(0, MF_BYPOSITION);
		SetWindowText(text + L" (Administrator)");
	}

	const int WindowMenuPosition = 6;

	CMenuHandle menuMain = GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WindowMenuPosition));
	m_view.SetTitleBarWindow(m_hWnd);

	InitMenu(menuMain);
	UIAddMenu(menuMain);

	if (AppSettings::Get().DarkMode())
		UISetCheck(ID_OPTIONS_DARKMODE, true);
	UISetCheck(ID_OPTIONS_SINGLEINSTANCE, AppSettings::Get().SingleInstance());
	SingleInstance::Register(m_hWnd);

	auto pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	SetAlwaysOnTop(AppSettings::Get().AlwaysOnTop());

	if (auto lf = AppSettings::Get().Font(); lf.lfFaceName[0])
		m_ViewFont.CreateFontIndirect(&lf);

	PostMessage(WM_COMMAND, ID_OBJECTS_OBJECTMANAGERNAMESPACE);
	PostMessage(WM_COMMAND, ID_OBJECTS_OBJECTTYPES);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
	SingleInstance::Unregister(m_hWnd);
	WINDOWPLACEMENT wp{ sizeof(wp) };
	GetWindowPlacement(&wp);
	AppSettings::Get().MainWindowPlacement(wp);
	AppSettings::Get().Save();
	CMessageLoop* pLoop = _Module.GetMessageLoop();
	ATLASSERT(pLoop != NULL);
	pLoop->RemoveMessageFilter(this);
	pLoop->RemoveIdleHandler(this);

	bHandled = FALSE;
	return 1;
}

void CMainFrame::SetAlwaysOnTop(bool alwaysOnTop) {
	SetWindowPos(alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
	UISetCheck(ID_OPTIONS_ALWAYSONTOP, alwaysOnTop);
}

LRESULT CMainFrame::OnAlwaysOnTop(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto alwaysOnTop = !(GetExStyle() & WS_EX_TOPMOST);
	SetAlwaysOnTop(alwaysOnTop);
	AppSettings::Get().AlwaysOnTop(alwaysOnTop);
	return 0;
}

HFONT CMainFrame::GetViewFont() const {
	return m_ViewFont.m_hFont;
}

LRESULT CMainFrame::OnOptionsFont(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	LOGFONT lf{};
	if (m_ViewFont)
		m_ViewFont.GetLogFont(lf);
	else {
		// start from the font the lists currently use
		CFontHandle font(AtlGetDefaultGuiFont());
		if (m_view.GetPageCount() > 0) {
			auto view = (IView*)m_view.GetPageData(m_view.GetActivePage());
			HWND hList = nullptr;
			::EnumChildWindows(view->GetHwnd(), [](HWND hWnd, LPARAM p) -> BOOL {
				WCHAR className[32];
				if (::GetClassName(hWnd, className, _countof(className)) && ::_wcsicmp(className, WC_LISTVIEW) == 0) {
					*(HWND*)p = hWnd;
					return FALSE;
				}
				return TRUE;
				}, (LPARAM)&hList);
			if (hList)
				font = (HFONT)::SendMessage(hList, WM_GETFONT, 0, 0);
		}
		if (font)
			font.GetLogFont(lf);
	}

	CFontDialog dlg(&lf, CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT, nullptr, m_hWnd);
	
	if (!WTLHelper::InvokeFontDialog(dlg, m_hWnd))
		return 0;

	CFont font;
	if (!font.CreateFontIndirect(&dlg.m_lf))
		return 0;

	//
	// set the new font before destroying the old one, which the controls still use
	//
	for (int i = 0; i < m_view.GetPageCount(); i++)
		ViewFactory::SetViewFont(((IView*)m_view.GetPageData(i))->GetHwnd(), font);
	m_ViewFont.Attach(font.Detach());
	AppSettings::Get().Font(dlg.m_lf);

	return 0;
}

LRESULT CMainFrame::OnFileExit(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	PostMessage(WM_CLOSE);
	return 0;
}

LRESULT CMainFrame::OnObjectTypes(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto view = ViewFactory::Get().CreateView(ViewType::ObjectTypes);
	return 0;
}

LRESULT CMainFrame::OnObjectManager(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto view = ViewFactory::Get().CreateView(ViewType::ObjectManager);
	return 0;
}

LRESULT CMainFrame::OnViewStatusBar(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto bVisible = !::IsWindowVisible(m_hWndStatusBar);
	::ShowWindow(m_hWndStatusBar, bVisible ? SW_SHOWNOACTIVATE : SW_HIDE);
	UISetCheck(ID_VIEW_STATUS_BAR, bVisible);
	UpdateLayout();
	return 0;
}

LRESULT CMainFrame::OnAppAbout(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CAboutDlg dlg;
	dlg.DoModal();
	return 0;
}

LRESULT CMainFrame::OnWindowClose(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nActivePage = m_view.GetActivePage();
	if (nActivePage != -1)
		m_view.RemovePage(nActivePage);
	else
		::MessageBeep((UINT)-1);

	return 0;
}

LRESULT CMainFrame::OnWindowCloseAll(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	m_view.RemoveAllPages();

	return 0;
}

LRESULT CMainFrame::OnWindowActivate(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	int nPage = wID - ID_WINDOW_TABFIRST;
	m_view.SetActivePage(nPage);
	ActivatePage(nPage);

	return 0;
}

LRESULT CMainFrame::OnRunAsAdmin(WORD, WORD, HWND, BOOL&) {
	// the elevated instance must not find this one (single instance) while it's still closing
	SingleInstance::Unregister(m_hWnd);
	if (SecurityHelper::RunElevated(nullptr, true)) {
		SendMessage(WM_CLOSE);
	}
	else {
		SingleInstance::Register(m_hWnd);
	}

	return 0;
}

LRESULT CMainFrame::OnSingleInstance(WORD, WORD, HWND, BOOL&) {
	auto& settings = AppSettings::Get();
	settings.SingleInstance(!settings.SingleInstance());
	UISetCheck(ID_OPTIONS_SINGLEINSTANCE, settings.SingleInstance());
	return 0;
}

LRESULT CMainFrame::OnFind(WORD, WORD, HWND, BOOL&) {
	CFindDlg dlg;
	if (dlg.DoModal(m_hWnd) == IDOK)
		ViewFactory::Get().CreateSearchView(dlg.GetText(), dlg.IsMatchCase());
	return 0;
}

LRESULT CMainFrame::OnNewView(WORD, WORD id, HWND, BOOL&) {
	ViewType type;
	switch (id) {
		case ID_OBJECTS_PIPES: type = ViewType::Pipes; break;
		case ID_OBJECTS_MAILSLOTS: type = ViewType::Mailslots; break;
		case ID_SYSTEM_PROCESSES: type = ViewType::Processes; break;
		case ID_SYSTEM_THREADS: type = ViewType::Threads; break;
		case ID_SYSTEM_SYSTEMINFORMATION: type = ViewType::SystemInformation; break;
		default: return 0;
	}
	ViewFactory::Get().CreateView(type);
	return 0;
}

LRESULT CMainFrame::OnPageActivated(int, LPNMHDR hdr, BOOL&) {
	auto page = static_cast<int>(hdr->idFrom);
	ActivatePage(page);

	return 0;
}

void CMainFrame::ActivatePage(int page) {
	if (m_CurrentPage >= 0 && m_CurrentPage < m_view.GetPageCount()) {
		((IView*)m_view.GetPageData(m_CurrentPage))->PageActivated(false);
		UIEnable(ID_FILE_SAVE, FALSE);
	}
	if (page >= 0) {
		auto view = (IView*)m_view.GetPageData(page);
		ATLASSERT(view);
		view->PageActivated(true);
	}
	m_CurrentPage = page;
}


#define ROUND_MEM(x) ((x + (1 << 17)) >> 18)

LRESULT CMainFrame::OnTimer(UINT /*uMsg*/, WPARAM id, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	if (id == 100) {
		static PERFORMANCE_INFORMATION pi = { sizeof(pi) };
		CString text;
		if (::GetPerformanceInfo(&pi, sizeof(pi))) {
			text.Format(L"Processes: %u", pi.ProcessCount);
			m_StatusBar.SetText(1, text);
			text.Format(L"Threads: %u", pi.ThreadCount);
			m_StatusBar.SetText(2, text);
			text.Format(L"Commit: %u / %u GB", ROUND_MEM(pi.CommitTotal), ROUND_MEM(pi.CommitLimit));
			m_StatusBar.SetText(3, text);
			text.Format(L"RAM Avail: %u / %u GB", ROUND_MEM(pi.PhysicalAvailable), ROUND_MEM(pi.PhysicalTotal));
			m_StatusBar.SetText(4, text);
		}
		ObjectManager::EnumTypes();
		text.Format(L"Handles: %lld / %lld", ObjectManager::TotalHandles, ObjectManager::PeakHandles);
		m_StatusBar.SetText(5, text);
		text.Format(L"Objects: %lld / %lld", ObjectManager::TotalObjects, ObjectManager::PeakObjects);
		m_StatusBar.SetText(6, text);
	}
	return 0;
}

void CMainFrame::SetStatusText(int index, PCWSTR text) {
	m_StatusBar.SetText(index, text);
}

LRESULT CMainFrame::OnAllHandles(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const {
	ViewFactory::Get().CreateView(ViewType::AllHandles);
	return 0;
}

LRESULT CMainFrame::OnAllObjects(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) const {
	if (!SecurityHelper::IsRunningElevated()) {
		AtlMessageBox(m_hWnd, L"Getting all objects requires running elevated", IDS_TITLE, MB_ICONWARNING);
		return 0;
	}
	ViewFactory::Get().CreateView(ViewType::Objects);
	return 0;
}

LRESULT CMainFrame::OnHandlesInProcess(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	CProcessSelectorDlg dlg;
	if (dlg.DoModal() == IDOK) {
		ViewFactory::Get().CreateView(ViewType::ProcessHandles, dlg.GetSelectedProcess());
	}
	return 0;
}

LRESULT CMainFrame::OnZombieProcesses(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ViewFactory::Get().CreateView(ViewType::ZombieProcesses);
	return 0;
}

LRESULT CMainFrame::OnZombieThreads(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	ViewFactory::Get().CreateView(ViewType::ZombieThreads);
	return 0;
}

LRESULT CMainFrame::OnAboutWindows(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	std::thread([this]() { ::ShellAbout(m_hWnd, L"Windows", nullptr, nullptr); }).detach();

	return 0;
}

LRESULT CMainFrame::OnShowWindow(UINT, WPARAM show, LPARAM, BOOL&) {
	static bool shown = false;
	if (show && !shown) {
		shown = true;
		auto wp = AppSettings::Get().MainWindowPlacement();
		if (wp.showCmd != SW_HIDE) {
			SetWindowPlacement(&wp);
			UpdateLayout();
		}
		if (AppSettings::Get().AlwaysOnTop())
			SetWindowPos(HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	return 0;
}

LRESULT CMainFrame::OnUpdateDarkMode(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();

	WTLHelper::SwitchToMode(settings.DarkMode() ? DarkModeKind::Dark : DarkModeKind::Light, m_hWnd);
	UISetCheck(ID_OPTIONS_DARKMODE, settings.DarkMode());
	InitMenu(GetMenu());
	DrawMenuBar();
	SendMessageToDescendants(WM_UPDATE_DARKMODE);
	SendMessageToDescendants(::RegisterWindowMessage(L"WTLHelperUpdateTheme"));

	return 0;
}

LRESULT CMainFrame::OnToggleDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto& settings = AppSettings::Get();
	settings.DarkMode(!settings.DarkMode());
	PostMessage(WM_UPDATE_DARKMODE, 0, 0);
	return 0;
}

LRESULT CMainFrame::OnMenuSelect(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	return 0;
}
