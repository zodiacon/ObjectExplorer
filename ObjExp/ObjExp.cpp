// ObjExp.cpp : main source file for ObjExp.exe
//

#include "pch.h"
#include "resource.h"
#include "MainFrm.h"
#include "SecurityHelper.h"
#include <ThemeHelper.h>
#include "AppSettings.h"
#include "DbgDriver.h"

CAppModule _Module;
AppSettings g_Settings;

int Run(LPTSTR /*lpstrCmdLine*/ = nullptr, int nCmdShow = SW_SHOWDEFAULT) {
	CMessageLoop theLoop;
	_Module.AddMessageLoop(&theLoop);

	CMainFrame wndMain;

	if (wndMain.CreateEx() == nullptr) {
		ATLTRACE(_T("Main window creation failed!\n"));
		return 0;
	}

	wndMain.ShowWindow(nCmdShow);

	int nRet = theLoop.Run();

	_Module.RemoveMessageLoop();
	return nRet;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR lpstrCmdLine, int nCmdShow) {
	SecurityHelper::EnablePrivilege(SE_DEBUG_NAME, true);
	SecurityHelper::EnablePrivilege(SE_SECURITY_NAME, true);

	HRESULT hRes = ::CoInitialize(nullptr);
	ATLASSERT(SUCCEEDED(hRes));

	AtlInitCommonControls(ICC_BAR_CLASSES | ICC_COOL_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES);

	hRes = _Module.Init(nullptr, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

	if (SecurityHelper::IsRunningElevated()) {
		DbgDriver::Get().Open();
	}
	ThemeHelper::Init();
	int nRet = Run(lpstrCmdLine, nCmdShow);

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
