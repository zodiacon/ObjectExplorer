#pragma once

#include <Settings.h>

class AppSettings : public Settings {
public:
	BEGIN_SETTINGS(AppSettings)
		SETTING(MainWindowPlacement, WINDOWPLACEMENT{}, SettingType::Binary);
		SETTING(Font, LOGFONT{}, SettingType::Binary);
		SETTING(AlwaysOnTop, 0, SettingType::Bool);
		SETTING(ViewToolBar, 1, SettingType::Bool);
		SETTING(ViewStatusBar, 1, SettingType::Bool);
		SETTING(DarkMode, 0, SettingType::Bool);
		SETTING(SingleInstance, 0, SettingType::Bool);
		SETTING(SymbolServerEnabled, 0, SettingType::Bool);
		SETTING_STRING(SymbolServerUrl, DefaultSymbolServerUrl);
		SETTING_STRING(SymbolCache, DefaultSymbolCache);
		SETTING_STRING(SymbolExtraPaths, L"");
		SETTING(SymbolUseEnvPath, 1, SettingType::Bool);
	END_SETTINGS

	DEF_SETTING(DarkMode, int)
	DEF_SETTING(AlwaysOnTop, int)
	DEF_SETTING(ViewToolBar, int)
	DEF_SETTING(ViewStatusBar, int)
	DEF_SETTING(SingleInstance, int)
	DEF_SETTING(MainWindowPlacement, WINDOWPLACEMENT)
	DEF_SETTING(Font, LOGFONT)
	DEF_SETTING(SymbolServerEnabled, int)
	DEF_SETTING_STRING(SymbolServerUrl)
	DEF_SETTING_STRING(SymbolCache)
	DEF_SETTING_STRING(SymbolExtraPaths)
	DEF_SETTING(SymbolUseEnvPath, int)

	static constexpr PCWSTR DefaultSymbolServerUrl = L"https://msdl.microsoft.com/download/symbols";
	static constexpr PCWSTR DefaultSymbolCache = L"%LOCALAPPDATA%\\ObjectExplorer\\Symbols";

	// the search path handed to DIA ("extra paths;_NT_SYMBOL_PATH;srv*cache*url") from the settings
	std::wstring BuildSymbolSearchPath() const;
	static std::wstring BuildSymbolSearchPath(bool useServer, std::wstring const& url, std::wstring const& cache,
		std::wstring const& extra, bool useEnv);
};

