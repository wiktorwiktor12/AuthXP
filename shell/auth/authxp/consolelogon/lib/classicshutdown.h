#pragma once
#include <windows.h>

//MODIFIED version of the classicshutdown header, for use with loadlibrary

/**
  * SDS_USER is passed to DisplayShutdownDialog to use the style from
  * registry. It can *not* be set in the registry.
  */
typedef enum _SHUTDOWNSTYLE
{
	SDS_USER = 0,
	SDS_WIN95,
	SDS_WIN98,
	SDS_WINME,
	SDS_WIN2K,
	SDS_WINXP,
	SDS_WINXP_GINA,
	SDS_WIN03_GINA,
	SDS_COUNT
} SHUTDOWNSTYLE;

/**
  * LOS_USER is passed to DisplayLogoffDialog to use the style from
  * registry. It *can* be set in registry, and if it is, then the shutdown
  * style will be mapped to one of these.
  */
typedef enum _LOGOFFSTYLE
{
	LOS_USER = 0,
	LOS_WIN98,
	LOS_WIN2K,
	LOS_WINXP,
	LOS_WINXP_GINA,
	LOS_COUNT
} LOGOFFSTYLE;

typedef enum _SHUTDOWNTYPE
{
	SHTDN_NONE        = 0,
	SHTDN_LOGOFF      = (1 << 0),
	SHTDN_SHUTDOWN    = (1 << 1),
	SHTDN_RESTART     = (1 << 2),
	SHTDN_RESTART_DOS = (1 << 3),
	SHTDN_SLEEP       = (1 << 4),
	SHTDN_HIBERNATE   = (1 << 5),
	SHTDN_DISCONNECT  = (1 << 6),
	SHTDN_ALL         = SHTDN_LOGOFF | SHTDN_SHUTDOWN | SHTDN_RESTART |
						SHTDN_RESTART_DOS |SHTDN_SLEEP | SHTDN_HIBERNATE |
						SHTDN_DISCONNECT
} SHUTDOWNTYPE;

/* Options for Windows 2000, XP GINA, and Server 2003 GINA styled shutdown dialogs. */
typedef struct _SHUTDOWNOPTIONS
{
	HBITMAP  hbmBrand;
	HBITMAP  hbmBar;
	BOOL     fSolidBanner;
	COLORREF crBanner;
} SHUTDOWNOPTIONS, *PSHUTDOWNOPTIONS;

inline HRESULT(WINAPI*	fDisplayShutdownDialog)(HWND hwndParent, SHUTDOWNSTYLE style, SHUTDOWNTYPE type, PSHUTDOWNOPTIONS pOptions) = nullptr;
inline HRESULT(WINAPI*	fDisplayLogoffDialog)(HWND hwndParent, LOGOFFSTYLE style) = nullptr;
inline HRESULT(WINAPI*	fDisplayDisconnectDialog)(HWND hwndParent) = nullptr;

typedef enum _DWORDSETTING
{
	CSDS_MIN = 0,
	CSDS_SHUTDOWNSTYLE = 0,
	CSDS_LOGOFFSTYLE,
	CSDS_SOLIDBANNER,
	CSDS_BANNERCOLOR,
	CSDS_SHUTDOWNSETTING,
	CSDS_SHUTDOWNTYPE,
	CSDS_COUNT
} DWORDSETTING;

inline HRESULT(WINAPI*	 fReadSettingDWORD)(DWORDSETTING setting, LPDWORD lpdwValue)= nullptr;
inline HRESULT(WINAPI*	 fWriteSettingDWORD)(DWORDSETTING setting, DWORD dwValue)= nullptr;

typedef enum _STRINGSETTING
{
	CSSS_MIN = 0,
	CSSS_BRANDBITMAP = 0,
	CSSS_BARBITMAP,
	CSSS_COUNT
} STRINGSETTING;

inline HRESULT(WINAPI*	 fReadSettingString)(STRINGSETTING setting, LPWSTR szBuffer, DWORD cchBuffer)= nullptr;
inline HRESULT(WINAPI*	 fWriteSettingString)(STRINGSETTING setting, LPCWSTR szBuffer)= nullptr;

HRESULT   DisplayShutdownDialog(HWND hwndParent, SHUTDOWNSTYLE style, SHUTDOWNTYPE type, PSHUTDOWNOPTIONS pOptions);
HRESULT     DisplayLogoffDialog(HWND hwndParent, LOGOFFSTYLE style);
HRESULT DisplayDisconnectDialog(HWND hwndParent);

HRESULT  ReadSettingDWORD(DWORDSETTING setting, LPDWORD lpdwValue);
HRESULT WriteSettingDWORD(DWORDSETTING setting, DWORD dwValue);

HRESULT  ReadSettingString(STRINGSETTING setting, LPWSTR szBuffer, DWORD cchBuffer);
HRESULT WriteSettingString(STRINGSETTING setting, LPCWSTR szBuffer);

HMODULE LoadClassicShutdownDll();

#ifdef __cplusplus

/* Templated helpers for (Read|Write)SettingDWORD */

template <typename T>
inline HRESULT ReadSettingDWORD(DWORDSETTING setting, T *lpValue)
{
	static_assert(sizeof(T) == sizeof(DWORD), "Size of templated argument to ReadSettingDWORD must be the same as DWORD");
	return ReadSettingDWORD(setting, (LPDWORD)lpValue);
}

template <typename T>
inline HRESULT WriteSettingDWORD(DWORDSETTING setting, T value)
{
	static_assert(sizeof(T) == sizeof(DWORD), "Size of templated argument to WriteSettingDWORD must be the same as DWORD");
	return WriteSettingDWORD(setting, (DWORD)value);
}

#endif
