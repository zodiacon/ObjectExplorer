
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
#include <ThemeHelper.h>
#include <VersionResourceHelper.h>
#include "AppSettings.h"

BOOL CMainFrame::PreTranslateMessage(MSG* pMsg) {
	if (CFrameWindowImpl<CMainFrame>::PreTranslateMessage(pMsg))
		return TRUE;

	return m_view.PreTranslateMessage(pMsg);
}

BOOL CMainFrame::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

void CMainFrame::InitMenu() {
	const struct {
		int id;
		UINT icon;
		HICON hIcon{ nullptr };
	} commands[] = {
		{ ID_EDIT_COPY, IDI_COPY },
		{ ID_EDIT_PASTE, IDI_PASTE },
		{ ID_EDIT_CUT, IDI_CUT },
		{ ID_OBJECTS_OBJECTTYPES, IDI_TYPES },
		{ ID_OBJECTS_OBJECTMANAGERNAMESPACE, IDI_PACKAGE },
		{ ID_FILE_RUNASADMINISTRATOR, 0, IconHelper::GetShieldIcon() },
		{ ID_OPTIONS_ALWAYSONTOP, IDI_PIN },
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
		{ ID_OBJECTS_HANDLESINPROCESS, IDI_MAGNET2 },
		{ ID_TYPESLIST_ALLHANDLES, IDI_MAGNET2 },
		{ ID_TYPESLIST_ALLOBJECTS, IDI_OBJECTS },
		{ ID_HANDLELIST_CLOSE, IDI_DELETE },
	};

	for (auto& cmd : commands) {
		if (cmd.icon)
			AddCommand(cmd.id, cmd.icon);
		else
			AddCommand(cmd.id, cmd.hIcon);
	}
}

HWND CMainFrame::GetHwnd() const {
	return m_hWnd;
}

BOOL CMainFrame::TrackPopupMenu(HMENU hMenu, DWORD flags, int x, int y) {
	return ShowContextMenu(hMenu, flags, x, y);
}

CUpdateUIBase& CMainFrame::GetUI() {
	return *this;
}

bool CMainFrame::AddToolBar(HWND tb) {
	return UIAddToolBar(tb);
}

LRESULT CMainFrame::OnCreate(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/) {
	InitDarkTheme();
	AppSettings::Get().LoadFromKey(L"SOFTWARE\\ScorpioSoftware\\ObjectExplorer");

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

	SetCheckIcon(AtlLoadIconImage(IDI_CHECK, 0, 16, 16), AtlLoadIconImage(IDI_RADIO, 0, 16, 16));

	const int WindowMenuPosition = 6;

	CMenuHandle menuMain = GetMenu();
	m_view.SetWindowMenu(menuMain.GetSubMenu(WindowMenuPosition));
	m_view.SetTitleBarWindow(m_hWnd);

	AddMenu(menuMain);
	InitMenu();
	UIAddMenu(menuMain);

	SetDarkMode(AppSettings::Get().DarkMode());

	auto pLoop = _Module.GetMessageLoop();
	pLoop->AddMessageFilter(this);
	pLoop->AddIdleHandler(this);

	SetAlwaysOnTop(AppSettings::Get().AlwaysOnTop());

	PostMessage(WM_COMMAND, ID_OBJECTS_OBJECTMANAGERNAMESPACE);
	PostMessage(WM_COMMAND, ID_OBJECTS_OBJECTTYPES);

	return 0;
}

LRESULT CMainFrame::OnDestroy(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& bHandled) {
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
	if (SecurityHelper::RunElevated(nullptr, true)) {
		SendMessage(WM_CLOSE);
	}

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

void CMainFrame::InitDarkTheme() {
	m_DarkTheme.BackColor = m_DarkTheme.SysColors[COLOR_WINDOW] = RGB(32, 32, 32);
	m_DarkTheme.TextColor = m_DarkTheme.SysColors[COLOR_WINDOWTEXT] = RGB(248, 248, 248);
	m_DarkTheme.SysColors[COLOR_HIGHLIGHT] = RGB(10, 10, 160);
	m_DarkTheme.SysColors[COLOR_HIGHLIGHTTEXT] = RGB(240, 240, 240);
	m_DarkTheme.SysColors[COLOR_MENUTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_CAPTIONTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_BTNFACE] = m_DarkTheme.BackColor;
	m_DarkTheme.SysColors[COLOR_BTNTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_3DLIGHT] = RGB(192, 192, 192);
	m_DarkTheme.SysColors[COLOR_BTNHIGHLIGHT] = RGB(192, 192, 192);
	m_DarkTheme.SysColors[COLOR_CAPTIONTEXT] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_3DSHADOW] = m_DarkTheme.TextColor;
	m_DarkTheme.SysColors[COLOR_SCROLLBAR] = m_DarkTheme.BackColor;
	m_DarkTheme.Name = L"Dark";
	m_DarkTheme.Menu.BackColor = m_DarkTheme.BackColor;
	m_DarkTheme.Menu.TextColor = m_DarkTheme.TextColor;
	m_DarkTheme.StatusBar.BackColor = m_DarkTheme.BackColor;
	m_DarkTheme.StatusBar.TextColor = m_DarkTheme.TextColor;
}

LRESULT CMainFrame::OnDarkMode(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/) {
	auto dark = !AppSettings::Get().DarkMode();
	AppSettings::Get().DarkMode(dark);
	SetDarkMode(dark);
	return 0;
}

void CMainFrame::SetDarkMode(bool dark) {
	ThemeHelper::SetCurrentTheme(dark ? m_DarkTheme : m_DefaultTheme, m_hWnd);
	ThemeHelper::UpdateMenuColors(*this, dark);
	UpdateMenu(GetMenu(), true);
	DrawMenuBar();
	UISetCheck(ID_OPTIONS_DARKMODE, dark);
}

