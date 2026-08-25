#include "pch.h"

#include <cinttypes>
#include <minidumpapiset.h>
#include <windows.h>
#include <wil/result.h>
#include <string>
#include <fstream>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iomanip>

using namespace Microsoft::WRL;

#pragma region "Pattern search stuff"

inline void SectionBeginAndSize(HMODULE hhh, const char* pszSectionName, PBYTE* beginSection, DWORD* sizeSection)
{
	*beginSection = nullptr;
	*sizeSection = 0;

	PIMAGE_DOS_HEADER dosHeader = (PIMAGE_DOS_HEADER)hhh;
	if (dosHeader->e_magic == IMAGE_DOS_SIGNATURE)
	{
		PIMAGE_NT_HEADERS64 ntHeader = (PIMAGE_NT_HEADERS64)((BYTE*)dosHeader + dosHeader->e_lfanew);
		if (ntHeader->Signature == IMAGE_NT_SIGNATURE)
		{
			PIMAGE_SECTION_HEADER firstSection = IMAGE_FIRST_SECTION(ntHeader);
			for (unsigned int i = 0; i < ntHeader->FileHeader.NumberOfSections; ++i)
			{
				PIMAGE_SECTION_HEADER section = firstSection + i;
				if (!strcmp((const char*)section->Name, pszSectionName))
				{
					*beginSection = (PBYTE)dosHeader + section->VirtualAddress;
					*sizeSection = section->SizeOfRawData;
					break;
				}
			}
		}
	}
}

inline void TextSectionBeginAndSize(HMODULE hhh, PBYTE* beginSection, DWORD* sizeSection)
{
	SectionBeginAndSize(hhh, ".text", beginSection, sizeSection);
}

inline BOOL MaskCompare(PVOID pBuffer, LPCSTR lpPattern, LPCSTR lpMask)
{
	for (PBYTE value = (PBYTE)pBuffer; *lpMask; ++lpPattern, ++lpMask, ++value)
	{
		if (*lpMask == 'x' && *(LPCBYTE)lpPattern != *value)
			return FALSE;
	}

	return TRUE;
}

inline __declspec(noinline) PVOID FindPatternHelper(PVOID pBase, SIZE_T dwSize, LPCSTR lpPattern, LPCSTR lpMask)
{
	for (SIZE_T index = 0; index < dwSize; ++index)
	{
		PBYTE pAddress = (PBYTE)pBase + index;

		if (MaskCompare(pAddress, lpPattern, lpMask))
			return pAddress;
	}

	return nullptr;
}

inline PVOID FindPattern(PVOID pBase, SIZE_T dwSize, LPCSTR lpPattern, LPCSTR lpMask)
{
	dwSize -= strlen(lpMask);
	return FindPatternHelper(pBase, dwSize, lpPattern, lpMask);
}

#pragma endregion "Pattern search stuff"

BOOL SHProcessMessagesUpdateTimeout(DWORD dwStartTick, DWORD dwTimeoutTotal, DWORD* pdwTimeoutRemaining)
{
    *pdwTimeoutRemaining = 0;
    BOOL bRet = TRUE;

    if (dwTimeoutTotal != -1)
    {
        DWORD dwWaitedTick = GetTickCount() - dwStartTick;
        if (dwWaitedTick <= dwTimeoutTotal)
        {
            *pdwTimeoutRemaining = dwTimeoutTotal - dwWaitedTick;
        }
        else
        {
            bRet = FALSE;
        }
    }
    else
    {
        *pdwTimeoutRemaining = -1;
    }

    return bRet;
}

BOOL PeekMessageWithWakeMask(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, BOOL bRemove, DWORD dwWakeMask)
{
    BOOL bRet = FALSE;
    UINT wRemoveMsg = 0;

    if (dwWakeMask == QS_ALLINPUT)
    {
        if (bRemove)
            wRemoveMsg |= PM_REMOVE;
        bRet = PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
    }
    else
    {
        if ((dwWakeMask & QS_INPUT) != 0)
            wRemoveMsg |= PM_QS_INPUT;

        if ((dwWakeMask & (QS_POSTMESSAGE | QS_ALLPOSTMESSAGE)) != 0)
            wRemoveMsg |= PM_QS_POSTMESSAGE;

        if ((dwWakeMask & QS_PAINT) != 0)
            wRemoveMsg |= PM_QS_PAINT;

        if ((dwWakeMask & QS_SENDMESSAGE) != 0)
            wRemoveMsg |= PM_QS_SENDMESSAGE;

        if (wRemoveMsg)
        {
            if (bRemove)
                wRemoveMsg |= PM_REMOVE;
            bRet = PeekMessageW(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, wRemoveMsg);
        }
    }

    return bRet;
}

DWORD SHProcessMessagesUntilEventsEx(HWND hwnd, HANDLE* pHandles, DWORD cHandles, DWORD dwTimeout, DWORD dwWakeMask, DWORD dwFlags)
{
    APTTYPE aptType;
    APTTYPEQUALIFIER aptTypeQualifier;
    BOOL fIsASTA = SUCCEEDED(CoGetApartmentType(&aptType, &aptTypeQualifier))
        && aptTypeQualifier == APTTYPEQUALIFIER_APPLICATION_STA
        && aptType == APTTYPE_STA;

    DWORD dwStartTick = GetTickCount();
    DWORD dwTimeoutRemaining = dwTimeout;

    if (!pHandles && dwTimeout == INFINITE)
    {
        return WAIT_FAILED;
    }

    DWORD dwReturn;

    do
    {
        do
        {
            if (fIsASTA)
            {
                HRESULT hrCoWait = CoWaitForMultipleHandles(
                    ((dwFlags & MWMO_INPUTAVAILABLE) != 0 ? COWAIT_INPUTAVAILABLE : 0)
                    | (COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES),
                    dwTimeoutRemaining, cHandles, pHandles, &dwReturn);
                if (hrCoWait == RPC_S_CALLPENDING)
                {
                    dwReturn = WAIT_TIMEOUT;
                }
                else if (FAILED(hrCoWait))
                {
                    dwReturn = WAIT_FAILED;
                }
            }
            else
            {
                dwReturn = MsgWaitForMultipleObjectsEx(cHandles, pHandles, dwTimeoutRemaining, dwWakeMask, dwFlags);
            }

            if (!SHProcessMessagesUpdateTimeout(dwStartTick, dwTimeout, &dwTimeoutRemaining))
            {
                dwReturn = WAIT_TIMEOUT;
            }

            if (dwReturn != cHandles)
            {
                break;
            }

            MSG msg;
            while (PeekMessageWithWakeMask(&msg, hwnd, 0, 0, TRUE, QS_ALLINPUT))
            {
                if (msg.message == WM_QUIT)
                {
                    PostQuitMessage((int)msg.wParam);
                    dwReturn = WAIT_TIMEOUT;
                    break;
                }

                TranslateMessage(&msg);
                if (msg.message == WM_SETCURSOR && LOWORD(msg.lParam) != HTERROR)
                {
                    SetCursor(LoadCursorW(nullptr, IDC_WAIT));
                }
                else
                {
                    DispatchMessageW(&msg);
                }

                if (cHandles)
                {
                    DWORD dwReturnTemp = WaitForMultipleObjectsEx(cHandles, pHandles, 0, 0, FALSE);
                    if (dwReturnTemp != WAIT_TIMEOUT)
                    {
                        dwReturn = dwReturnTemp;
                        break;
                    }
                }

                if (!SHProcessMessagesUpdateTimeout(dwStartTick, dwTimeout, &dwTimeoutRemaining))
                {
                    dwReturn = WAIT_TIMEOUT;
                    break;
                }
            }
        }
        while (dwReturn == cHandles);
    }
    while (dwReturn == WAIT_IO_COMPLETION);

    if (dwReturn == WAIT_TIMEOUT && cHandles)
    {
        DWORD dwReturnTemp = WaitForMultipleObjectsEx(cHandles, pHandles, 0, 0, FALSE);
        if (dwReturnTemp != WAIT_TIMEOUT)
        {
            dwReturn = dwReturnTemp;
        }
    }

    return dwReturn;
}

namespace Windows::Internal::ComTaskPool
{
	volatile DWORD s_dwUniqueCallingContext = 0x80000000;
	volatile DWORD* p_s_dwThreadIdReuse; // Used in the actual code
}

DWORD WINAPI CUSTOM_SHTaskPoolGetUniqueContext()
{
	return InterlockedIncrement(&Windows::Internal::ComTaskPool::s_dwUniqueCallingContext);
}

void WINAPI CUSTOM_SHTaskPoolAllowThreadReuse()
{
	if (Windows::Internal::ComTaskPool::p_s_dwThreadIdReuse)
		*Windows::Internal::ComTaskPool::p_s_dwThreadIdReuse = GetCurrentThreadId();
}

void TaskPool_InitFunctions()
{
	HMODULE hShcore = LoadLibraryW(L"shcore.dll");
	SHTaskPoolQueueTask = (decltype(SHTaskPoolQueueTask))GetProcAddress(hShcore, "SHTaskPoolQueueTask");
	SHTaskPoolGetUniqueContext = (decltype(SHTaskPoolGetUniqueContext))GetProcAddress(hShcore, "SHTaskPoolGetUniqueContext");
	SHTaskPoolAllowThreadReuse = (decltype(SHTaskPoolAllowThreadReuse))GetProcAddress(hShcore, "SHTaskPoolAllowThreadReuse");

	PBYTE textBegin;
	DWORD textSize;
	TextSectionBeginAndSize(hShcore, &textBegin, &textSize);

	if (!SHTaskPoolQueueTask)
	{
		// HRESULT Windows::Internal::ComTaskPool::RunTask(
		//     Windows::Internal::TaskApartment apartment,
		//     Windows::Internal::TaskOptions options,
		//     DWORD dwCallingContext,
		//     DWORD dwMillisecondsFromNow,
		//     Windows::Internal::IComPoolTask* pTask,
		//     IUnknown** ppDelayedTask)
		// 44 8B C0 8B 0F 48 89 5C 24 ?? E8 ?? ?? ?? ??
		//                                  ^^^^^^^^^^^
		// Ref: Windows::Internal::ComTaskPoolHandler::Start()
		// Arguments dwMillisecondsFromNow and ppDelayedTask are optimized out; always 0 and nullptr respectively
#if defined(_M_X64)
		PBYTE matchRunTask = (PBYTE)FindPattern(
			textBegin,
			textSize,
			"\x44\x8B\xC0\x8B\x0F\x48\x89\x5C\x24\x00\xE8",
			"xxxxxxxxx?x"
		);
		if (matchRunTask)
		{
			matchRunTask += 10;
			matchRunTask += 5 + *(int*)(matchRunTask + 1);
		}
#else
		PBYTE matchRunTask = nullptr;
#endif
		if (matchRunTask)
		{
			SHTaskPoolQueueTask = (decltype(SHTaskPoolQueueTask))matchRunTask;
			printf("Windows::Internal::ComTaskPool::RunTask() = shcore.dll+%" PRIXPTR "\n", (PBYTE)SHTaskPoolQueueTask - (PBYTE)hShcore);
		}
		FAIL_FAST_IF_NULL(SHTaskPoolQueueTask);
	}

	if (!SHTaskPoolGetUniqueContext)
	{
		SHTaskPoolGetUniqueContext = CUSTOM_SHTaskPoolGetUniqueContext;
	}
}

#if !defined(__WRL_CLASSIC_COM__)
STDAPI DllGetActivationFactory(_In_ HSTRING activatibleClassId, _COM_Outptr_ IActivationFactory** factory)
{
	return Module<InProc>::GetModule().GetActivationFactory(activatibleClassId, factory);
}
#endif

#if !defined(__WRL_WINRT_STRICT__)
STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, _COM_Outptr_ void** ppv)
{
	return Module<InProc>::GetModule().GetClassObject(rclsid, riid, ppv);
}
#endif

STDAPI DllCanUnloadNow()
{
	return Module<InProc>::GetModule().Terminate() ? S_OK : S_FALSE;
}

STDAPI_(BOOL) DllMain(_In_opt_ HINSTANCE hInstance, DWORD dwReason, _In_opt_ void* __formal)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hInstance);
		TaskPool_InitFunctions();
	}
	return TRUE;
}
