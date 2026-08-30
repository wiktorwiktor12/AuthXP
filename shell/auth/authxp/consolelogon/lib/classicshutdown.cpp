#include "pch.h"
#include "classicshutdown.h"

#include <string>

#include "logonframe.h"

HRESULT DisplayShutdownDialog(HWND hwndParent, SHUTDOWNSTYLE style, SHUTDOWNTYPE type, PSHUTDOWNOPTIONS pOptions)
{
	if (!fDisplayShutdownDialog)
	{
		fDisplayShutdownDialog = reinterpret_cast<decltype(fDisplayShutdownDialog)>(GetProcAddress(LoadClassicShutdownDll(), "DisplayShutdownDialog"));
	}
	return fDisplayShutdownDialog(hwndParent, style, type, pOptions);
}

HRESULT DisplayLogoffDialog(HWND hwndParent, LOGOFFSTYLE style)
{
	if (!fDisplayLogoffDialog)
	{
		fDisplayLogoffDialog = reinterpret_cast<decltype(fDisplayLogoffDialog)>(GetProcAddress(LoadClassicShutdownDll(), "DisplayLogoffDialog"));
	}
	return fDisplayLogoffDialog(hwndParent, style);
}

HRESULT DisplayDisconnectDialog(HWND hwndParent)
{
	if (!fDisplayDisconnectDialog)
	{
		fDisplayDisconnectDialog = reinterpret_cast<decltype(fDisplayDisconnectDialog)>(
			GetProcAddress(LoadClassicShutdownDll(), "DisplayDisconnectDialog"));
	}
	return fDisplayDisconnectDialog(hwndParent);
}

HRESULT ReadSettingDWORD(DWORDSETTING setting, LPDWORD lpdwValue)
{
	if (!fReadSettingDWORD)
	{
		fReadSettingDWORD = reinterpret_cast<decltype(fReadSettingDWORD)>(GetProcAddress(LoadClassicShutdownDll(), "ReadSettingDWORD"));
	}
	return fReadSettingDWORD(setting, lpdwValue);
}

HRESULT WriteSettingDWORD(DWORDSETTING setting, DWORD dwValue)
{
	if (!fWriteSettingDWORD)
	{
		fWriteSettingDWORD = reinterpret_cast<decltype(fWriteSettingDWORD)>(GetProcAddress(LoadClassicShutdownDll(), "WriteSettingDWORD"));
	}
	return fWriteSettingDWORD(setting, dwValue);
}

HRESULT ReadSettingString(STRINGSETTING setting, LPWSTR szBuffer, DWORD cchBuffer)
{
	if (!fReadSettingString)
	{
		fReadSettingString = reinterpret_cast<decltype(fReadSettingString)>(GetProcAddress(LoadClassicShutdownDll(), "ReadSettingString"));
	}
	return fReadSettingString(setting, szBuffer, cchBuffer);
}

HRESULT WriteSettingString(STRINGSETTING setting, LPCWSTR szBuffer)
{
	if (!fWriteSettingString)
	{
		fWriteSettingString = reinterpret_cast<decltype(fWriteSettingString)>(GetProcAddress(LoadClassicShutdownDll(), "WriteSettingString"));
	}
	return fWriteSettingString(setting, szBuffer);
}

HMODULE LoadClassicShutdownDll()
{
	static HMODULE ClassicShutdownDll = nullptr;
	if (!ClassicShutdownDll)
	{
		ClassicShutdownDll = LoadLibraryW(g_plf->settingClassicShutdownPath);
		return ClassicShutdownDll;
	}

	return ClassicShutdownDll;
}
