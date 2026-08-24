#include "pch.h"

#include "consoleuimanager.h"

#include <cassert>
#include <uxtheme.h>

#include "advisablebutton.h"
#include "animationstrip.h"
#include "backgroundwindow.h"
#include "combobox.h"
#include "labeledcheckbox.h"
#include "logonaccount.h"
#include "logonaccountlist.h"
#include "logonframe.h"
#include "restrictededit.h"
#include "DirectUI/DirectUI.h"
#include "ShellScalingApi.h"
#include "zoomableelement.h"

using namespace Microsoft::WRL;

ConsoleUIManager::ConsoleUIManager()
	: m_UIThreadInitResult(E_FAIL)
{
}

HRESULT ConsoleUIManager::Initialize()
{
	// m_UIThreadQuitEvent = nullptr; // @MOD Don't need this line
	m_UIThreadQuitEvent.reset(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
	return m_UIThreadQuitEvent ? S_OK : ResultFromKnownLastError();
}

HRESULT ConsoleUIManager::StartUI()
{
	auto scopeExit = wil::scope_exit([this]() -> void { StopUI(); });

	Wrappers::SRWLock::SyncLockExclusive lock = m_lock.LockExclusive();

	if (!m_UIThreadHandle)
	{
		wil::unique_handle quitEvent(CreateEventExW(nullptr, nullptr, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS));
		RETURN_LAST_ERROR_IF_NULL(quitEvent); // 43

		m_UIThreadQuitEvent = std::move(quitEvent);

		RETURN_IF_WIN32_BOOL_FALSE(SHCreateThreadWithHandle(
			s_UIThreadHostThreadProc,
			this,
			CTF_COINIT,
			s_UIThreadHostStartThreadProc,
			&m_UIThreadHandle)); // 52
	}

	scopeExit.release();
	return S_OK;
}

HRESULT ConsoleUIManager::StopUI()
{
	Wrappers::SRWLock::SyncLockExclusive lock = m_lock.LockExclusive();
	if (m_UIThreadHandle)
	{
		SetEvent(m_UIThreadQuitEvent.get());

		DWORD dwIndex = 0;
		CoWaitForMultipleHandles(COWAIT_ALERTABLE | COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES, INFINITE, 1, m_UIThreadHandle.addressof(), &dwIndex);
		m_UIThreadHandle.reset();

		ResetEvent(m_UIThreadQuitEvent.get());
		m_UIThreadInitResult = E_FAIL;
	}

	return S_OK;
}

HRESULT ConsoleUIManager::EnsureUIStarted()
{
	RETURN_HR_IF(E_ABORT, !m_Dispatcher.Get());
	return S_OK;
}

DWORD ConsoleUIManager::s_UIThreadHostStartThreadProc(void* parameter)
{
	auto pThis = static_cast<ConsoleUIManager*>(parameter);
	pThis->AddRef();
	pThis->UIThreadHostStartThreadProc();
	return 0;
}

HRESULT ConsoleUIManager::UIThreadHostStartThreadProc()
{
	HRESULT hr;
	auto scopeExit = wil::scope_exit([&]() -> void { m_UIThreadInitResult = hr; });

	RETURN_IF_FAILED(hr = MakeNotificationDispatcher<CNotificationDispatcher>(&m_Dispatcher)); // 219

	return S_OK;
}

DWORD ConsoleUIManager::s_UIThreadHostThreadProc(void* parameter)
{
	auto pThis = static_cast<ConsoleUIManager*>(parameter);

	HRESULT hr = S_OK;
	if (SUCCEEDED(pThis->m_UIThreadInitResult))
	{
		hr = pThis->UIThreadHostThreadProc();
	}
	pThis->Release();
	return hr;
}

void CALLBACK LogonParseError(LPCWSTR pszError, LPCWSTR pszToken, int dLine, void* pContext)
{
	WCHAR buf[201];

	if (dLine != -1)
		swprintf(buf, 201,L"%s '%s' at line %d", pszError, pszToken, dLine);
	else
		swprintf(buf, 201, L"%s '%s'", pszError, pszToken);

	MessageBoxW(NULL, buf, L"Parser Message", MB_OK);
}

DWORD ConsoleUIManager::UIThreadHostThreadProc()
{
	DWORD dwIndex = WAIT_IO_COMPLETION;

	HANDLE waitHandles[] = { m_UIThreadQuitEvent.get() };

	SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
	CoInitializeEx(nullptr, 0);
	DirectUI::InitProcessPriv(14, HINST_THISCOMPONENT, false, true, true);

	DirectUI::InitThread(2);
	DirectUI::RegisterAllControls();

	CBackgroundWindow   backgroundWindow(HINST_THISCOMPONENT);

	//CDUIAnimationStrip::Register();
	//CLogonFrame7::Register();
	LogonFrame::Register();
	LogonAccount::Register();
	LogonAccountList::Register();
	//CDUIUserTileElement::Register();
	//CDUIZoomableElement::Register();
	CDUIRestrictedEdit::Register();
	CDUIComboBox::Register();
	//UserList::Register();
	CAdvisableButton::Register();
	CDUIFieldContainer::Register();
	CDUILabeledCheckbox::Register();


	DirectUI::DUIXmlParser* pParser = NULL;
	DirectUI::NativeHWNDHost* pnhh = NULL;
	DirectUI::DisableAnimations();

    // Create host
    HMONITOR hMonitor;
    POINT pt;
    MONITORINFO monitorInfo;

    // Determine initial size of the host. This is desired to be the entire
    // primary monitor resolution because the host always runs on the secure
    // desktop. If magnifier is brought up it will call SHAppBarMessage which
    // will change the work area and we will respond to it appropriately from
    // the listener in shgina that sends us HM_DISPLAYRESIZE messages.

    pt.x = pt.y = 0;
    hMonitor = MonitorFromPoint(pt, MONITOR_DEFAULTTOPRIMARY);
    assert(hMonitor != NULL, "NULL HMONITOR returned from MonitorFromPoint");
    monitorInfo.cbSize = sizeof(monitorInfo);
    if (GetMonitorInfo(hMonitor, &monitorInfo) == FALSE)
    {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &monitorInfo.rcMonitor, 0);
    }
#define NHHO_IgnoreClose          1  // Ignore WM_CLOSE (i.e. Alt-F4, 'X' button), must be closed via DestroyWindow
    DirectUI::NativeHWNDHost::Create(L"Windows Logon", backgroundWindow.Create(), NULL, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
        monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top, 0, WS_POPUP, NHHO_IgnoreClose, &pnhh);
    //int initWidth = 1280;
    //int initHeight = 720;
//
    //RECT rcWindow;
    //rcWindow.left = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - initWidth) / 2;
    //rcWindow.top = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - initHeight) / 2;
    //rcWindow.right = rcWindow.left + initWidth;
    //rcWindow.bottom = rcWindow.top + initHeight;
//
    //DirectUI::NativeHWNDHost::Create(L"Windows Logon", NULL, NULL, rcWindow.left, rcWindow.top,
    //    initWidth, initHeight, 0, WS_POPUP, NHHO_IgnoreClose, &pnhh);
    if (!pnhh)
        goto Failure;

    // Populate handle list for theme style parsing
    g_rgH[0] = HINST_THISCOMPONENT; // Default HINSTANCE
    g_rgH[SCROLLBARHTHEME] = OpenThemeData(pnhh->GetHWND(), L"Scrollbar");

    // Frame creation

    DirectUI::DUIXmlParser::Create(&pParser, nullptr, nullptr, LogonParseError, nullptr);

    if (!pParser)
        goto Failure;

    if (FAILED(pParser->SetXMLFromResource(MAKEINTRESOURCE(IDR_LOGONUI), HINST_THISCOMPONENT, 0)))
        goto Failure;


    {

        // Always double buffer
        LogonFrame::Create(pnhh->GetHWND(), true, 0, 0,(DirectUI::Element**)&g_plf);
        if (!g_plf)
        {
            goto Failure;
        }

        g_plf->SetNativeHost(pnhh);

        DirectUI::Element* pe;
        pParser->CreateElement(L"main", g_plf, 0,0,&pe);

        if (pe) // Fill contents using substitution
        {
            // Frame tree is built
            if (FAILED(g_plf->OnTreeReady(pParser)))
            {
                goto Failure;
            }
        	g_plf->EnterPreStatusMode(false);
            //if (fShutdownLaunch || fWait)
            //{
            //    g_plf->SetTitle(IDS_PLEASEWAIT);
            //}
//
            //if (!fStatusLaunch)
            //{
            //    // Build contents of account list
            //    g_plf->EnterLogonMode(false);
            //}
            //else
            //{
            //    g_plf->EnterPreStatusMode(false);
            //}

            // Host
            pnhh->Host(g_plf);

            g_plf->SetButtonLabels();

            DirectUI::Element* peLogoArea = g_plf->FindDescendent(DirectUI::StrToID(L"product"));

            //if (!g_fNoAnimations)
            //{
            //    pnhh->ShowWindow(SW_SHOW);
            //    DoFadeWindow(pnhh->GetHWND());
            //    if (peLogoArea)
            //    {
            //        peLogoArea->SetAlpha(0);
            //    }
            //}

            // Set visible and focus
            g_plf->SetVisible(true);
            g_plf->SetKeyFocus();

            // Do initial show
            pnhh->ShowWindow(SW_SHOW);

            //if (!g_fNoAnimations)
            //{
            //    DirectUI::EnableAnimations();
            //}

            if (peLogoArea)
            {
                peLogoArea->SetAlpha(255);
            }
            //g_plf->HideDateTimeArea();
            //g_plf->ShowDateTimeArea();
            //g_plf->EnterSecurityOptionsMode();
            //g_plf->SetTitle(TEXT("HI"));

            //g_plf->SetStatus(L"Updating transformation... Stage 2 of 5 - 5% complete.\nDo not turn off your computer.");

            //DirectUI::StartMessagePump();

            // psf will be deleted by native HWND host when destroyed
        }
    }

	while (dwIndex == WAIT_IO_COMPLETION)
	{
		MSG Msg {};
		while ( PeekMessageW(&Msg, nullptr, 0, 0, PM_REMOVE) )
		{
			if (Msg.message == WM_QUIT)
			{
				break;
			}

			TranslateMessage(&Msg);
			DispatchMessageW(&Msg);
		}

		CoWaitForMultipleHandles(
			COWAIT_ALERTABLE | COWAIT_INPUTAVAILABLE | COWAIT_DISPATCH_CALLS | COWAIT_DISPATCH_WINDOW_MESSAGES,
			INFINITE, ARRAYSIZE(waitHandles), waitHandles, &dwIndex);
	}

	goto ok;

Failure:
	MessageBox(0,L"Failure",L"Failure",0);

ok:
	if (pnhh)
		pnhh->Destroy();
	if (pParser)
		pParser->Destroy();

	//DirectUI::ReleaseStatusHost();

	//DirectUI::FreeLayoutInfo(LAYOUT_DEF_USER);

	if (g_rgH[SCROLLBARHTHEME])  // Scrollbar
	{
		CloseThemeData(g_rgH[SCROLLBARHTHEME]);
	}

	if (m_Dispatcher.Get())
	{
		ComPtr<IObjectWithBackReferences> objWithBackRefs;
		if (SUCCEEDED(m_Dispatcher.As(&objWithBackRefs)))
		{
			objWithBackRefs->RemoveBackReferences();
		}

		m_Dispatcher.Reset();
	}

	DirectUI::UnInitThread();
	DirectUI::UnInitProcessPriv(HINST_THISCOMPONENT);
	CoUninitialize();

	//FreeConsole();
	return 0;
}
