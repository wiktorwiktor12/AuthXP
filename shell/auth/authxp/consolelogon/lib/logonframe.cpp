#include "pch.h"
#include "logonframe.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <WtsApi32.h>
#include <uxtheme.h>

#include "logoninterfaces.h"
#include "duiutil.h"
#include "errorballoon.h"
#include "logonaccount.h"
#include "logonguids.h"
#include "slpublic.h"
#include "powrprof.h"

using namespace Microsoft::WRL;

#define ICC_WINLOGON_REINIT    0x80000000
void PokeComCtl32()
{
	INITCOMMONCONTROLSEX iccex = { sizeof(INITCOMMONCONTROLSEX), ICC_WINLOGON_REINIT | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES };
	InitCommonControlsEx(&iccex);
}

#define UNLEN       256                 // Maximum user name length
WCHAR szLastSelectedName[UNLEN + sizeof('\0')] = { L'\0' };


LogonFrame* g_plf = NULL;
BOOL g_fNoAnimations = false;

bool IsShutdownAllowed()
{
	Microsoft::WRL::ComPtr<IShutdownChoices> shutdownChoices;
	if (SUCCEEDED(CoCreateInstance(CLSID_AuthUIShutdownChoices, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&shutdownChoices))))
	{
		return shutdownChoices->UserHasShutdownRights() != 0;
	}

	return false;
}

//TODO: check if theres a proper easy windows way to check this
bool IsUndockAllowed()
{
	return false;
}

LPCWSTR LoadResString(UINT nID)
{
	static WCHAR szRes[101];
	szRes[0] = NULL;
	LoadStringW(g_plf->GetHInstance(), nID, szRes, _ARRAYSIZE(szRes) - 1);
	return szRes;
}

void LogonFrame::EnterPreStatusMode(BOOL fLock)
{
	if (IsPreStatusLock())
	{
		assert(!fLock, "Receiving a lock while already within pre-Status lock");
		return;
	}

	if (fLock)
	{
		LogonAccount* pAccount;
		// Entering pre-Status mode with "lock", cannot exit to logon state without an unlock
		_fPreStatusLock = TRUE;
		pAccount = static_cast<LogonAccount*>(_peAccountList->GetSelection());
		if (pAccount != NULL)
		{
			lstrcpynW(szLastSelectedName, pAccount->GetUsername(), ARRAYSIZE(szLastSelectedName));
		}
	}

	if (GetState() == LAS_Hide)
	{
		_pnhh->ShowWindow(SW_SHOW);
		SetWindowPos(_pnhh->GetHWND(), NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOMOVE | SWP_NOZORDER);

	}
	DWORD cookie;
	StartDefer(&cookie);

	SetKeyFocus();  // Removes selection

	HidePowerButton();
	HideUndockButton();
	ShowLogoArea();
	HideWelcomeArea();
	HideAccountPanel();

	Element* pe;
	pe = FindDescendent(DirectUI::StrToID(L"instruct"));
	assert(pe);
	pe->SetVisible(FALSE);

	SetStatus(LoadResString(IDS_WINDOWSNAME));

	EndDefer(cookie);

	// Set state
	SetState(LAS_PreStatus);
}

void LogonFrame::EnterLogonMode(BOOL fUnLock)
{
    // If currently locked, ignore call if not to unlock
    if (IsPreStatusLock())
    {
        if (fUnLock)
        {
            // Exiting pre-Status mode lock
            _fPreStatusLock = FALSE;
        }
        else
            return;
    }
    else
    {
        //assert(!fUnLock, "Receiving an unlock while not within pre-Status lock");
    }

    assert(GetState() != LAS_Hide, "Cannot enter logon state from hidden state");

    ResetTheme();

    Element* pe;

    DWORD cookie;
    StartDefer(&cookie);

    PokeComCtl32();

    // Retrieve data from backend if not populated
    if (UserListAvailable())
    {
        ResetUserList();
    }
    else
    {
        // Cache password field atoms for quicker identification (static)
        LogonAccount::idPwdGo = AddAtomW(L"go");

        LogonAccount::idPwdInfo = AddAtomW(L"info");

    }

    DirectUI::StyledScrollViewer* secOpts = (DirectUI::StyledScrollViewer*)FindDescendent(DirectUI::StrToID(L"SecurityOptions"));
    secOpts->SetLayoutPos(DirectUI::LP_None);

	DirectUI::Element* sas = FindDescendent(DirectUI::StrToID(L"sas"));
	sas->SetLayoutPos(DirectUI::LP_None);
	sas->SetVisible(FALSE);

	FindDescendent(DirectUI::StrToID(L"scroller"))->SetVisible(TRUE);
	FindDescendent(DirectUI::StrToID(L"scroller"))->SetLayoutPos(DirectUI::BLP_Left);
	FindDescendent(DirectUI::StrToID(L"accountlist"))->SetLayoutPos(DirectUI::LP_Auto);

    if (IsShutdownAllowed())
    {
        ShowPowerButton();
    }
    else
    {
        HidePowerButton();
    }

    if (IsUndockAllowed())
    {
        ShowUndockButton();
    }
    else
    {
        HideUndockButton();
    }

	ShowLogoArea();
	HideWelcomeArea();

    pe = FindDescendent(DirectUI::StrToID(L"instruct"));
    assert(pe);
    pe->SetVisible(TRUE);


    pe = FindDescendent(DirectUI::StrToID(L"product"));
    assert(pe);
    pe->StopAnimation(DirectUI::ANI_AlphaType);
    pe->RemoveLocalValue(BackgroundProp);

    // Account list viewer

    ShowAccountPanel();

    SetTitle(IDS_WELCOME);
    SetStatus(LoadResString(IDS_BEGIN),false);

	SetKeyFocus();

    EndDefer(cookie);

    // Set state
    SetState(LAS_Logon);

    SetButtonLabels();
    SetForegroundWindow(_pnhh->GetHWND());

}

void LogonFrame::EnterPostStatusMode()
{
    // Set state
    SetState(LAS_PostStatus);

    Element* pe;
    pe = FindDescendent(DirectUI::StrToID(L"instruct"));
    //assertNoMsg(pe);
    pe->SetVisible(FALSE);

    //animation was started in OnLogUserOn
    ShowWelcomeArea();
    HideLogoArea();
}

void LogonFrame::EnterHideMode()
{
    SetState(LAS_Hide);

    if (_pnhh)
    {
        _pnhh->HideWindow();
    }
}

void LogonFrame::EnterDoneMode()
{
    SetState(LAS_Done);

    if (_pnhh)
    {
        _pnhh->DestroyWindow();
    }
}

void LogonFrame::EnterSecurityOptionsMode(LC::LogonUISecurityOptions options, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion)
{
	m_SecurityOptionsCompletion = wil::make_unique_nothrow<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>>>(completion);
	if (m_SecurityOptionsCompletion.get() == nullptr)
		return;

	SetTitle(IDS_SECURITYOPTIONS);
	SetStatus(LoadResString(IDS_SECURITYOPTIONS),false);

    HideWelcomeArea();
    ShowLogoArea();
    ShowAccountPanel();

    if (IsShutdownAllowed())
        ShowPowerButton();

    if (IsUndockAllowed())
        ShowUndockButton();

    auto pe = FindDescendent(DirectUI::StrToID(L"instruct"));
    assert(pe);
    pe->SetVisible(TRUE);


    pe = FindDescendent(DirectUI::StrToID(L"product"));
    assert(pe);
    pe->StopAnimation(DirectUI::ANI_AlphaType);
    pe->RemoveLocalValue(BackgroundProp);

    SetButtonLabels();
    SetForegroundWindow(_pnhh->GetHWND());

    SetUserListAvailable(true);
    ResetUserList();
    SetUserListAvailable(false);

    FindDescendent(DirectUI::StrToID(L"scroller"))->SetVisible(FALSE);
    FindDescendent(DirectUI::StrToID(L"scroller"))->SetLayoutPos(-3);
    FindDescendent(DirectUI::StrToID(L"accountlist"))->SetLayoutPos(-3);

	DirectUI::Element* sas = FindDescendent(DirectUI::StrToID(L"sas"));
	sas->SetLayoutPos(DirectUI::LP_None);
	sas->SetVisible(FALSE);

    DirectUI::StyledScrollViewer* secOpts = (DirectUI::StyledScrollViewer*)FindDescendent(DirectUI::StrToID(L"SecurityOptions"));

    FindDescendent(DirectUI::StrToID(L"divider"))->SetLayoutPos(-1);
    secOpts->SetLayoutPos(DirectUI::BLP_Client);
    secOpts->SetVisible(TRUE);
    //secOpts->SetYScrollable(TRUE);


	bool showLock = (options & LC::LogonUISecurityOptions_Lock) != 0;
    FindDescendent(DirectUI::StrToID(L"SecurityLock"))->SetVisible(showLock ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"SecurityLock"))->SetLayoutPos(showLock ? -1 : -3);

	bool showSwitchUser = (options & LC::LogonUISecurityOptions_SwitchUser) != 0;
    FindDescendent(DirectUI::StrToID(L"SecuritySwitchUser"))->SetVisible(showSwitchUser ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"SecuritySwitchUser"))->SetLayoutPos(showSwitchUser ? -1 : -3);

	bool showLogOff = (options & LC::LogonUISecurityOptions_LogOff) != 0;
    FindDescendent(DirectUI::StrToID(L"SecurityLogOff"))->SetVisible(showLogOff ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"SecurityLogOff"))->SetLayoutPos(showLogOff ? -1 : -3);

	bool showChange = (options & LC::LogonUISecurityOptions_ChangePassword) != 0;
    FindDescendent(DirectUI::StrToID(L"SecurityChange"))->SetVisible(showChange ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"SecurityChange"))->SetLayoutPos(showChange ? -1 : -3);

	bool showTaskMan = (options & LC::LogonUISecurityOptions_TaskManager) != 0;
    FindDescendent(DirectUI::StrToID(L"SecurityTaskManager"))->SetVisible(showTaskMan ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"SecurityTaskManager"))->SetLayoutPos(showTaskMan ? -1 : -3);

	//bool showSecCancel = (options & LC::LogonUISecurityOptions_Cancel) != 0;
	bool showSecCancel = TRUE;
    FindDescendent(DirectUI::StrToID(L"CancelSecurityOptions"))->SetVisible(showSecCancel ? TRUE : FALSE);
    FindDescendent(DirectUI::StrToID(L"CancelSecurityOptions"))->SetLayoutPos(showSecCancel ? -1 : -3);

    //FindDescendent(DirectUI::StrToID(L"DialogButtons"))->SetVisible(TRUE);
    //FindDescendent(DirectUI::StrToID(L"DialogButtons"))->SetLayoutPos(-1);
}

HRESULT LogonFrame::OnSecurityOptionSelected(LC::LogonUISecurityOptions SecurityOpt)
{
	ComPtr<LC::ILogonUISecurityOptionsResultFactory> factory;
	RETURN_IF_FAILED(WF::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_LogonUISecurityOptionsResult).Get(), &factory)); // 101

	ComPtr<LC::ILogonUISecurityOptionsResult> optionResult;
	RETURN_IF_FAILED(factory->CreateSecurityOptionsResult(SecurityOpt, LC::LogonUIShutdownChoice_None, &optionResult)); // 104

	RETURN_IF_FAILED(m_SecurityOptionsCompletion->GetResult().Set(optionResult.Get())); // 106

	m_SecurityOptionsCompletion->Complete(S_OK);
	m_SecurityOptionsCompletion.reset();

	return S_OK;
}

HRESULT LogonFrame::ConfirmEmergencyShutdown()
{
	ComPtr<LC::ILogonUISecurityOptionsResultFactory> factory;
	RETURN_IF_FAILED(WF::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_LogonUISecurityOptionsResult).Get(), &factory)); // 101

	ComPtr<LC::ILogonUISecurityOptionsResult> optionResult;
	RETURN_IF_FAILED(factory->CreateSecurityOptionsResult(LC::LogonUISecurityOptions_Cancel, LC::LogonUIShutdownChoice_EmergencyRestart, &optionResult)); // 104

	RETURN_IF_FAILED(m_SecurityOptionsCompletion->GetResult().Set(optionResult.Get())); // 106

	m_SecurityOptionsCompletion->Complete(S_OK);
	m_SecurityOptionsCompletion.reset();

	return S_OK;
}

HRESULT LogonFrame::ShowLockedScreen()
{
	FindDescendent(DirectUI::StrToID(L"scroller"))->SetVisible(FALSE);
	FindDescendent(DirectUI::StrToID(L"scroller"))->SetLayoutPos(DirectUI::LP_None);
	FindDescendent(DirectUI::StrToID(L"accountlist"))->SetLayoutPos(DirectUI::LP_None);

	DirectUI::StyledScrollViewer* secOpts = (DirectUI::StyledScrollViewer*)FindDescendent(DirectUI::StrToID(L"SecurityOptions"));

	//FindDescendent(DirectUI::StrToID(L"divider"))->SetLayoutPos(DirectUI::LP_Auto);
	secOpts->SetLayoutPos(DirectUI::LP_None);
	secOpts->SetVisible(FALSE);

	DirectUI::Element* sas = FindDescendent(DirectUI::StrToID(L"sas"));
	sas->SetLayoutPos(DirectUI::BLP_Left);
	sas->SetVisible(TRUE);

	DirectUI::Element* keyboardicon = FindDescendent(DirectUI::StrToID(L"keyboardicon"));
	keyboardicon->SetVisible(TRUE);

	auto pv = DirectUI::Value::CreateGraphic(MAKEINTRESOURCEW(IDI_KEYBOARD), (USHORT)PointToPixel(36), (USHORT)PointToPixel(36), HINST_THISCOMPONENT,false,false);
	if (pv)
	{
		keyboardicon->SetValue(DirectUI::Element::ContentProp, DirectUI::PI_Local, pv);
		pv->Release();
	}
	SetStatus(L"",false);
	ShowAccountPanel();

	Element* pe = FindDescendent(DirectUI::StrToID(L"instruct"));
	assert(pe);
	pe->SetVisible(FALSE);

	HidePowerButton();
	HideUndockButton();

	return S_OK;
}

HRESULT LogonFrame::DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	int result = MessageBoxW(GetHWND(),messageContent,messageCaptionContent,MB_ICONINFORMATION);

	int returnFlag = 1;
	switch (result)
	{
	case IDOK:
		returnFlag = 1;
		break;
	case IDCANCEL:
		returnFlag = 2;
		break;
	case IDYES:
		returnFlag = 6;
		break;
	case IDNO:
		returnFlag = 7;
		break;
	default:
		returnFlag = 1;
	}

	auto m_MessageDisplayResultCompletion = wil::make_unique_nothrow<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>>>(completion);

	ComPtr<LC::IMessageDisplayResultFactory> factory;
	RETURN_IF_FAILED(WF::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_MessageDisplayResult).Get(), &factory));

	ComPtr<LC::IMessageDisplayResult> messageResult;
	RETURN_IF_FAILED(factory->CreateMessageDisplayResult(returnFlag, &messageResult));

	RETURN_IF_FAILED(m_MessageDisplayResultCompletion->GetResult().Set(messageResult.Get()));

	m_MessageDisplayResultCompletion->Complete(S_OK);
}

LRESULT LogonFrame::InteractiveLogonRequest(LPCWSTR pszUsername, LPCWSTR pszPassword)
{
    LRESULT lResult = 0;
    LogonAccount* pla;
    pla = FindNamedUser(pszUsername);

    if (pla)
    {
        if (pla->OnAuthenticateUser())
        {
            lResult = ERROR_SUCCESS;
        }
        else
        {
            lResult = ERROR_ACCESS_DENIED;
        }
    }
    return(lResult);
}

void LogonFrame::NextFlagAnimate(DWORD dwFrame)
{
	//if (settingShouldAnimateFlag == FALSE)
	//	UNREFERENCED_PARAMETER(dwFrame);
	//else
	{
		Element* pe;

		if (dwFrame >= MAX_FLAG_FRAMES || g_fNoAnimations)
		{
			return;
		}

		pe = FindDescendent(DirectUI::StrToID(L"product"));
		assert(pe);

		if (pe)
		{
			HBITMAP hbm = NULL;
			HDC hdc;
			DirectUI::Value* pv = NULL;

			hdc = CreateCompatibleDC(_hdcAnimation);

			if (hdc)
			{
				pv = pe->GetValue(Element::ContentProp, DirectUI::PI_Local,0);
				if (pv)
				{
					hbm = (HBITMAP)pv->GetImage(false,1.0f);
				}

				if (hbm)
				{
					_dwFlagFrame = dwFrame;
					if (_dwFlagFrame >= MAX_FLAG_FRAMES)
					{
						_dwFlagFrame = 0;
					}


					HBITMAP hbmSave = (HBITMAP)SelectObject(hdc, hbm);
					BitBlt(hdc, 0, 0, 137, 86, _hdcAnimation, 0, 86 * _dwFlagFrame, SRCCOPY);
					SelectObject(hdc, hbmSave);

					HGADGET hGad = pe->GetDisplayNode();
					if (hGad)
					{
						InvalidateGadget(hGad);
					}
				}

				if (pv)
				{
					pv->Release();
				}
				DeleteDC(hdc);
			}
		}
	}

}

void LogonFrame::DisplaySerializationFailed(HSTRING caption, HSTRING message)
{
	HWND target = this->GetHWND();
	if (_peLogonAccountFocused && _peLogonAccountFocused->_pePwdEdit)
		target = _peLogonAccountFocused->_pePwdEdit->GetHWND();

	auto rawCaption = WindowsGetStringRawBuffer(caption,NULL);
	auto rawmessage = WindowsGetStringRawBuffer(message,NULL);

	WCHAR captionContent[256];
	wcscpy_s(captionContent,rawCaption);

	WCHAR messageContent[256];
	wcscpy_s(messageContent,rawmessage);

	g_pErrorBalloon.ShowToolTip(GetModuleHandleW(NULL), target, messageContent, captionContent, TTI_ERROR, EB_WARNINGCENTERED | EB_MARKUP, 10000);
}

static HRESULT SHRegGetDWORDW(HKEY hkey, const WCHAR* pszSubKey, const WCHAR* pszValue, void* pvData)
{
	DWORD pcbData[6];
	pcbData[0] = 4;
	LSTATUS ValueW_0 = SHRegGetValueW(hkey, pszSubKey, pszValue, 16, nullptr, pvData, pcbData);
	HRESULT result = static_cast<WORD>(ValueW_0) | 0x80070000;
	if (ValueW_0 <= 0)
		return ValueW_0;
	return result;
}

void LogonFrame::LoadSettings()
{
	settingShouldAnimateFlag = FALSE;
	settingShouldShowDateAndTime = FALSE;
	SHRegGetDWORDW(HKEY_LOCAL_MACHINE,L"Software\\AuthXP",L"ShouldAnimateFlag",&settingShouldAnimateFlag);
	SHRegGetDWORDW(HKEY_LOCAL_MACHINE,L"Software\\AuthXP",L"ShouldShowDateAndTime",&settingShouldShowDateAndTime);
}

LogonFrame::~LogonFrame()
{
    if (_pvHotList)
        _pvHotList->Release();
    if (_pvList)
        _pvList->Release();
    if (_hdcAnimation)
        DeleteDC(_hdcAnimation);
    g_plf = NULL;
}

HRESULT LogonFrame::Initialize(HWND hParent, BOOL fDblBuffer, UINT nCreate)
{
    // Zero-init members
    _peAccountList = NULL;
    _peViewer = NULL;
    _peRightPanel = NULL;
    _peLeftPanel = NULL;
    _pbPower = NULL;
    _pbUndock = NULL;
    _peHelp = NULL;
    _peMsgArea = NULL;
    _peLogoArea = NULL;
    _peDateTimeArea = NULL;
    _pParser = NULL;
    _hwndNotification = NULL;
    _nStatusID = 0;
    _fPreStatusLock = FALSE;
    _nAppState = LAS_PreStatus;
    _pnhh = NULL;
    _fListAvailable = FALSE;
    _pvHotList = NULL;
    _pvList = NULL;
    _hdcAnimation = NULL;
    _dwFlagFrame = 0;
    _nColorDepth = 0;

	LoadSettings();
    // Set up notification window
    _hwndNotification = CreateWindowEx(0,
        TEXT("LogonWnd"),
        TEXT("Logon"),
        WS_OVERLAPPED,
        0, 0,
        10,
        10,
        HWND_MESSAGE,
        NULL,
        GetModuleHandleW(NULL),
        NULL);

    //if (SUCCEEDED(CoCreateInstance(CLSID_ShellLogonStatusHost, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARG(ILogonStatusHost, &g_pILogonStatusHost))))
    //{
    //    g_pILogonStatusHost->Initialize(GetModuleHandleW(NULL), _hwndNotification);
    //}

    // In status (pre) state
    SetState(LAS_PreStatus);

    // Do base class initialization
    HRESULT hr;
    HDC hDC = NULL;

    hr = HWNDElement::Initialize(hParent, fDblBuffer ? true : false, nCreate,nullptr,nullptr);
    if (FAILED(hr))
    {
        return hr;
        goto Failure;
    }

    if (!g_fNoAnimations)
    {
        // Initialize
        hDC = GetDC(NULL);
        _nDPI = GetDeviceCaps(hDC, LOGPIXELSY);
        _nColorDepth = GetDeviceCaps(hDC, BITSPIXEL);
        ReleaseDC(NULL, hDC);

    	if (settingShouldAnimateFlag == TRUE)
    	{
    		hDC = GetDC(hParent);
    		_hdcAnimation = CreateCompatibleDC(hDC);
    		if (_hdcAnimation)
    		{
    			_hbmpFlags = (HBITMAP)LoadImage(HINST_THISCOMPONENT, MAKEINTRESOURCE(IDB_FLAGSTRIP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
    			if (_hbmpFlags)
    			{
    				HBITMAP hbmOld = (HBITMAP)SelectObject(_hdcAnimation, _hbmpFlags);
    				DeleteObject(hbmOld);
    			}
    			else
    			{
    				DeleteDC(_hdcAnimation);
    				_hdcAnimation = NULL;
    			}
    		}
    		ReleaseDC(hParent, hDC);
    	}
    }

    hr = SetActive(DirectUI::AEF_MouseAndKeyboard);
    if (FAILED(hr))
        goto Failure;

    return S_OK;


Failure:

    return hr;
}

LogonAccount* LogonFrame::InternalFindNamedUser(LPCWSTR pszUsername)
{
    LogonAccount* plaResult = NULL;
    DirectUI::Value* pvChildren;

    auto peList = _peAccountList->GetChildren(&pvChildren);
    if (peList)
    {
        for (UINT i = 0; i < peList->GetSize(); i++)
        {
            assert(peList->GetItem(i)->GetClassInfo() == LogonAccount::Class, "Account list must contain LogonAccount objects");

            LogonAccount* pla = (LogonAccount*)peList->GetItem(i);

            if (pla)
            {
                if (lstrcmpi(pla->GetUsername(), pszUsername) == 0)
                {
                    plaResult = pla;
                    break;
                }
            }
        }
    }

    pvChildren->Release();
    return plaResult;
}

void LogonFrame::UpdateUserStatus(BOOL fRefreshAll)
{
    DirectUI::Value* pvChildren;
    static bool fUpdating = false;
    // Early out if:    no user list available
    //                  not in logon mode (showing user list)

    if (!UserListAvailable() || (GetState() != LAS_Logon) || fUpdating)
        return;

    fUpdating = true;
    DWORD cookie;
    StartDefer(&cookie);

    auto peList = _peAccountList->GetChildren(&pvChildren);
    if (peList)
    {
        for (UINT i = 0; i < peList->GetSize(); i++)
        {
            assert(peList->GetItem(i)->GetClassInfo() == LogonAccount::Class, "Account list must contain LogonAccount objects");

            LogonAccount* pla = (LogonAccount*)peList->GetItem(i);

            if (pla)
            {
                pla->UpdateNotifications(fRefreshAll);
            }
        }
    }

    if (IsUndockAllowed())
    {
        ShowUndockButton();
    }
    else
    {
        HideUndockButton();
    }

    pvChildren->Release();
    EndDefer(cookie);
    fUpdating = false;
}

void LogonFrame::UpdateTime()
{
	SYSTEMTIME st;
	GetLocalTime(&st);

	if (st.wMinute == _lastMinute)
		return;

	WCHAR buffer[64];
	int result;
	
	result = GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &st, nullptr, buffer, 64);
	if (result == 0)
		result = GetTimeFormatW(LOCALE_SYSTEM_DEFAULT, TIME_NOSECONDS, &st, nullptr, buffer, 64);
		
	if (result == 0)
		_peTimeArea->SetContentString(nullptr);
	else
		_peTimeArea->SetContentString(buffer);

	result = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_LONGDATE, &st, nullptr, buffer, 64);
	if (result == 0)
		result = GetDateFormatW(LOCALE_SYSTEM_DEFAULT, DATE_LONGDATE, &st, nullptr, buffer, 64);

	if (result == 0)
		_peDateArea->SetContentString(nullptr);
	else
		_peDateArea->SetContentString(buffer);

	_lastMinute = st.wMinute;
}

LogonAccount* LogonFrame::FindUserByCred(Microsoft::WRL::ComPtr<LCPD::ICredential>& cred)
{
	LogonAccount* plaResult = NULL;
	DirectUI::Value* pvChildren;

	auto peList = _peAccountList->GetChildren(&pvChildren);
	if (peList)
	{
		for (UINT i = 0; i < peList->GetSize(); i++)
		{
			assert(peList->GetItem(i)->GetClassInfo() == LogonAccount::Class, "Account list must contain LogonAccount objects");

			LogonAccount* pla = (LogonAccount*)peList->GetItem(i);

			if (pla)
			{
				if (pla->_tileData == cred)
				{
					plaResult = pla;
					break;
				}
			}
		}
	}

	pvChildren->Release();
	return plaResult;
}

LogonAccount* LogonFrame::FindNamedUser(LPCWSTR pszUsername)
{
    // Early out if:    no user list available
    //                  not in logon mode (showing user list)

    if (!UserListAvailable() || (GetState() != LAS_Logon))
    {
        return NULL;
    }
    else
    {
        return(InternalFindNamedUser(pszUsername));
    }
}

void LogonFrame::SelectUser(LPCWSTR pszUsername)
{
    LogonAccount* pla;

    pla = FindNamedUser(pszUsername);
    if (pla != NULL)
    {
        pla->OnAuthenticatedUser();
    }
    else
    {
        LogonAccount::ClearCandidate();
        EnterPostStatusMode();
        HidePowerButton();
        HideUndockButton();
        HideAccountPanel();
    }
}

void LogonFrame::ResetUserList()
{
    if (UserListAvailable())
    {
        // reset the candidate to NULL
        LogonAccount::ClearCandidate();

        // remove of the password panel from the current account (if any)
        SetKeyFocus();

        //fix up the existing list to get us back into logon mode
        DirectUI::Value* pvChildren;
        auto peList = _peAccountList->GetChildren(&pvChildren);

        if (peList)
        {
            LogonAccount* peAccount;
            for (int i = peList->GetSize() - 1; i >= 0; i--)
            {
                peAccount = (LogonAccount*)peList->GetItem(i);
                peAccount->Destroy();
            }
        }
        pvChildren->Release();
    }
}

void LogonFrame::Resize(BOOL fWorkArea)
{
    RECT rc;
    SIZE size;
    static BOOL fWorkAreaChanged = FALSE;

    if (fWorkArea)
    {
        fWorkAreaChanged = TRUE;
    }

    if (fWorkAreaChanged)
    {
        SystemParametersInfo(SPI_GETWORKAREA, 0, &rc, 0);
        size.cx = rc.right - rc.left;
        size.cy = rc.bottom - rc.top;
    }
    else
    {
        rc.left = 0;
        rc.top = 0;
        size.cx = GetSystemMetrics(SM_CXSCREEN);
        size.cy = GetSystemMetrics(SM_CYSCREEN);
    }

    SetWindowPos(_pnhh->GetHWND(),
        NULL,
        rc.left,
        rc.top,
        size.cx,
        size.cy,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_ASYNCWINDOWPOS);
}

void LogonFrame::SetAnimations(BOOL fAnimations)
{
    g_fNoAnimations = !fAnimations;
    if (fAnimations)
    {
        DirectUI::EnableAnimations();
    }
    else
    {
        DirectUI::DisableAnimations();
    }
}
HANDLE g_rgH[3] = { 0 };
void LogonFrame::ResetTheme()
{
    DirectUI::DUIXmlParser* pParser = NULL;
    DirectUI::Value* pvScrollerSheet;
    Element* peListScroller = NULL;
    if (g_rgH[SCROLLBARHTHEME])
    {
        CloseThemeData(g_rgH[SCROLLBARHTHEME]);
        g_rgH[SCROLLBARHTHEME] = NULL;
    }

    g_rgH[SCROLLBARHTHEME] = OpenThemeData(_pnhh->GetHWND(), L"Scrollbar");

    //Parser::Create(IDR_LOGONUI, g_rgH, LogonParseError, &pParser);
    //if (pParser && !pParser->WasParseError())
    //{
    //    pvScrollerSheet = pParser->GetSheet(L"scroller");
//
    //    if (pvScrollerSheet)
    //    {
    //        peListScroller = (Selector*)FindDescendent(StrToID(L"scroller"));
//
    //        peListScroller->SetValue(SheetProp, PI_Local, pvScrollerSheet);
//
    //        pvScrollerSheet->Release();
     //   }
//
     //   pParser->Destroy();
    //}
}
DirectUI::IClassInfo* LogonFrame::Class = NULL;
HRESULT LogonFrame::Register()
{
    return DirectUI::ClassInfo<LogonFrame, HWNDElement, DirectUI::EmptyCreator<LogonFrame>>::Register(L"LogonFrame", NULL, 0);
}

void LogonFrame::SetStatus(LPCWSTR psz, bool bHideAccountPanel)
{
	if (psz)
	{
		_peHelp->SetContentString(psz);

		//we can make the welcome text change, but it prob should stay always as welcome like xp
		/*std::wstring lowerString = psz;
		std::transform(lowerString.begin(), lowerString.end(), lowerString.begin(),
			  ::tolower);

		_peMsgArea->FindDescendent(DirectUI::StrToID(L"welcome"))->SetContentString(lowerString.c_str());
		_peMsgArea->FindDescendent(DirectUI::StrToID(L"welcomeshadow"))->SetContentString(lowerString.c_str());*/
		if (GetState() != LAS_PostStatus && bHideAccountPanel)
		{
			HidePowerButton();
			HideUndockButton();
			HideAccountPanel();
			Element* pe = FindDescendent(DirectUI::StrToID(L"instruct"));
			pe->SetVisible(FALSE);
		}
		if (GetState() == LAS_PostStatus && LogonAccount::_peCandidate)
		{
			LogonAccount::_peCandidate->SetStatus(0,psz);
			LogonAccount::_peCandidate->ShowStatus(0);
		}
	}
}

void LogonFrame::SetTitle(UINT uRCID)
{
    WCHAR sz[1024];
    ZeroMemory(&sz, sizeof(sz));

    if (_nStatusID != uRCID)
    {

#ifdef DBG
        int cRead = 0;
        cRead = LoadStringW(_pParser->GetHInstance(), uRCID, sz, DUIARRAYSIZE(sz));
        assert(cRead, "Could not locate string resource ID");
#else
        LoadStringW(_pParser->GetHInstance(), uRCID, sz, ARRAYSIZE(sz));
#endif

        SetTitle(sz);
        _nStatusID = uRCID;
    }
}

void LogonFrame::SetTitle(LPCWSTR pszTitle)
{
    Element* peTitle = NULL, * peShadow = NULL;

    peTitle = (DirectUI::Button*)FindDescendent(DirectUI::StrToID(L"welcome"));
    assert(peTitle, "Cannot find title text, check the UI file");

    if (peTitle)
    {
        peShadow = (DirectUI::Button*)FindDescendent(DirectUI::StrToID(L"welcomeshadow"));
        assert(peShadow, "Cannot find title shadow text, check the UI file");
    }

    if (peTitle && peShadow)
    {
        peTitle->SetContentString(pszTitle);
        peShadow->SetContentString(pszTitle);
    }
}
#define MAX_COMPUTERDESC_LENGTH 255

#define SGCDNF_NOCACHEDENTRY    0x00000001
#define SGCDNF_DESCRIPTIONONLY  0x00010000


//STDAPI SHGetComputerDisplayNameW(LPCWSTR pszMachineName, DWORD dwFlags, LPWSTR pszDisplay, DWORD cchDisplay);
void LogonFrame::SetButtonLabels()
{
    WCHAR szComputerName[MAX_COMPUTERDESC_LENGTH + 1] = { 0 };
    DWORD cchComputerName = MAX_COMPUTERDESC_LENGTH + 1;

    static HRESULT(WINAPI * SHGetComputerDisplayName)(LPCWSTR pszMachineName, LPCWSTR outMachineName, DWORD dwFlags, LPWSTR pszDisplay, DWORD cchDisplay) = (decltype(SHGetComputerDisplayName))(GetProcAddress(LoadLibraryW(L"shell32.dll"),(LPCSTR)752));

    if (_pbPower && SUCCEEDED(SHGetComputerDisplayName(NULL, NULL, SGCDNF_DESCRIPTIONONLY, szComputerName, cchComputerName)))
    {
        WCHAR szCommand[MAX_COMPUTERDESC_LENGTH + 50], szRes[50];

        LoadStringW(g_plf->GetHInstance(), IDS_POWERNAME, szRes, ARRAYSIZE(szRes));
        wsprintf(szCommand, szRes, szComputerName);
        SetPowerButtonLabel(szCommand);

        LoadStringW(g_plf->GetHInstance(), IDS_UNDOCKNAME, szRes, ARRAYSIZE(szRes));
        wsprintf(szCommand, szRes, szComputerName);
        SetUndockButtonLabel(szCommand);
    }
}

HRESULT LogonFrame::AddAccountFromTile(const Microsoft::WRL::ComPtr<LCPD::ICredential>& tileData, const Microsoft::WRL::ComPtr<LCPD::IUser>& user, OUT LogonAccount** ppla)
{
    HRESULT hr;
    LogonAccount* pla = NULL;
	Wrappers::HString userName;
	BOOLEAN isLoggedOn = FALSE;
	Wrappers::HString hint;

    if (!_pParser)
    {
        hr = E_FAIL;
        goto Failure;
    }

    // Build up an account and insert into selection list
    hr = _pParser->CreateElement(L"accountitem", NULL, 0, 0, (Element**)&pla);
    if (FAILED(hr))
        goto Failure;

	//LOG_HR_MSG(E_FAIL,"user %p",user.Get());

	if (user)
	{
		RETURN_IF_FAILED(user->get_DisplayName(userName.ReleaseAndGetAddressOf()));

		RETURN_IF_FAILED(user->get_IsLoggedIn(&isLoggedOn));
	}
	else
	{
		RETURN_IF_FAILED(tileData->get_LogoLabel(userName.ReleaseAndGetAddressOf()));
	}

	//TODO: figure out a way to retrieve the password hint nicely, a hacky way to do it would be to find the credentialfield with it, but a way to find it no matter the display language would be needed
	hint.Set(L"");

    pla->_tileData = tileData;
    pla->_pUser = user;

    pla->_pParser = _pParser;
    hr = pla->OnTreeReady(false, userName.GetRawBuffer(0), userName.GetRawBuffer(0), hint.GetRawBuffer(0), TRUE, isLoggedOn, GetHInstance());
    if (FAILED(hr))
        goto Failure;

    hr = _peAccountList->Add(pla);
    if (FAILED(hr))
        goto Failure;

    if (pla)
    {
        SetElementAccessability(pla, true, ROLE_SYSTEM_LISTITEM, userName.GetRawBuffer(0));
    }

    if (_nColorDepth <= 8)
    {
        pla->SetBackgroundColor(ORGB(96, 128, 255));

        Element* pEle;
        pEle = pla->FindDescendent(DirectUI::StrToID(L"userpane"));
        if (pEle)
        {
            pEle->SetBorderColor(ORGB(96, 128, 255));
        }
    }

	pla->UpdateNotifications(true);

    if (ppla)
        *ppla = pla;

    return S_OK;


Failure:

    return hr;
}

int LogonFrame::_nDPI = 0;

HRESULT LogonFrame::Create(Element** ppElement)
{
    UNREFERENCED_PARAMETER(ppElement);
    assert("Cannot instantiate an HWND host derived Element via parser. Must use substitution.");
    return E_NOTIMPL;
}

HRESULT LogonFrame::Create(HWND hParent, BOOL fDblBuffer, UINT nCreate, DWORD* pdwDeferCookie, Element** ppElement)
{
    *ppElement = NULL;

    LogonFrame* plf = DirectUI::HNew<LogonFrame>();
    if (!plf)
        return E_OUTOFMEMORY;

    HRESULT hr = plf->Initialize(hParent, fDblBuffer, nCreate);
    if (FAILED(hr))
    {
        plf->Destroy();
        return hr;
    }

    *ppElement = plf;

    return S_OK;
}

void LogonFrame::OnEvent(DirectUI::Event* pEvent)
{
    if (pEvent->nStage == DirectUI::GMF_BUBBLED)  // Bubbled events
    {
        //g_pErrorBalloon.HideToolTip();
        if (pEvent->uidType == DirectUI::Button::Click)
        {
            if (pEvent->peTarget == _pbPower)
            {
                // Power button pressed
                OnPower();

                pEvent->fHandled = true;
                return;
            }
            else if (pEvent->peTarget == _pbUndock)
            {
                // Undock button pressed
                OnUndock();

                pEvent->fHandled = true;
                return;
            }
        }
    	if (pEvent->uidType == DirectUI::Button::Click() && m_SecurityOptionsCompletion && pEvent->nStage == DirectUI::GMF_BUBBLED)
    	{
    		LC::LogonUISecurityOptions options = LC::LogonUISecurityOptions_Cancel;
    		if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"SecurityLock"))
    			options = LC::LogonUISecurityOptions_Lock;
    		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"SecuritySwitchUser"))
    			options = LC::LogonUISecurityOptions_SwitchUser;
    		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"SecurityLogOff"))
    			options = LC::LogonUISecurityOptions_LogOff;
    		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"SecurityChange"))
    			options = LC::LogonUISecurityOptions_ChangePassword;
    		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"SecurityTaskManager"))
    			options = LC::LogonUISecurityOptions_TaskManager;
    		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"CancelSecurityOptions"))
    			options = LC::LogonUISecurityOptions_Cancel;
    		else
    		{
    			HWNDElement::OnEvent(pEvent);
    			return;
    		}

    		//if (m_bIsInEmergencyRestartDialog == false)
    			OnSecurityOptionSelected(options);
    	}
    	if (pEvent->uidType == DirectUI::Button::Click() && pEvent->nStage == DirectUI::GMF_BUBBLED)
    	{
    		if (IsElementOfClass(pEvent->peTarget,L"LogonAccount"))
    		{
    			LogonAccount* tile = static_cast<LogonAccount*>(pEvent->peTarget);
    			if (tile->_tileData.Get())
    			{
    				ComPtr<IInspectable> inspectable;
    				LOG_IF_FAILED(tile->_tileData.As(&inspectable));
    				LOG_IF_FAILED(m_consoleUIManager->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->_pUser.Get() ? tile->_pUser.Get() : inspectable.Get()));
    				LOG_HR_MSG(E_FAIL,"PUTTING THING");
    			}
    			else
    			{
    				LOG_HR_MSG(E_FAIL,"TILE HAD NO CREDENTIAL DATASOURCE");
    			}
    		}
    	}
    }

    HWNDElement::OnEvent(pEvent);
}

void LogonFrame::OnInput(DirectUI::InputEvent* pEvent)
{
    if (pEvent->nStage == DirectUI::GMF_DIRECT || pEvent->nStage == DirectUI::GMF_BUBBLED)
    {
        if (pEvent->nDevice == DirectUI::GINPUT_KEYBOARD)
        {
            DirectUI::KeyboardEvent* pke = (DirectUI::KeyboardEvent*)pEvent;
            if (pke->nCode == DirectUI::GKEY_DOWN)
            {
                switch (pke->ch)
                {
                case VK_ESCAPE:
                    g_pErrorBalloon.HideToolTip();
                    SetKeyFocus();
                    _peAccountList->SetSelection(NULL);
                    pEvent->fHandled = true;
                    return;

                case VK_UP:
                case VK_DOWN:
                    if (UserListAvailable())
                    {
                        if (_peAccountList->GetSelection() == NULL)
                        {
                            DirectUI::Value* pvChildren;
                            auto peList = _peAccountList->GetChildren(&pvChildren);
                            if (peList)
                            {
                                LogonAccount* peAccount = (LogonAccount*)peList->GetItem(0);
                                if (peAccount)
                                {
                                    peAccount->SetKeyFocus();
                                    _peAccountList->SetSelection(peAccount);
                                }
                            }
                            pvChildren->Release();
                            pEvent->fHandled = true;
                            return;
                        }
                    }
                    break;
                }
            }
        }
    }

    HWNDElement::OnInput(pEvent);
}

void LogonFrame::OnPropertyChanged(DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld,
    DirectUI::Value* pvNew)
{
    if (IsProp(KeyFocused))
    {
        if (pvNew->GetBool())
        {
            // Unselect items from account list if pressed on background
            _peAccountList->SetSelection(NULL);
        }
    }

    HWNDElement::OnPropertyChanged(ppi, iIndex, pvOld, pvNew);
}

DirectUI::Element* LogonFrame::GetAdjacent(Element* peFrom, int iNavDir, DirectUI::NavReference const* pnr,
    bool bKeyable)
{
    Element* peFound = HWNDElement::GetAdjacent(peFrom, iNavDir, pnr, bKeyable);

    if ((peFound == this))
    {
        // Don't let the frame show up in the tab order. Just repeat the search when we encounter the frame
        return HWNDElement::GetAdjacent(this, iNavDir, pnr, bKeyable);
    }

    return peFound;
}

HRESULT LogonFrame::OnLogUserOn(LogonAccount* pla)
{
    DWORD cookie;
    StartDefer(&cookie);

#ifdef GADGET_ENABLE_GDIPLUS

    // Disable status so that it can't be clicked on anymore
    pla->DisableStatus(0);
    pla->DisableStatus(1);

    // Clear list of logon accounts except the one logging on
    DirectUI::Value* pvChildren;
    auto peList = _peAccountList->GetChildren(&pvChildren);
    if (peList)
    {
        LogonAccount* peAccount;
        for (UINT i = 0; i < peList->GetSize(); i++)
        {
            peAccount = (LogonAccount*)peList->GetItem(i);

            if (peAccount != pla)
            {
                peAccount->SetLogonState(LS_Denied);
            }
            else
            {
                peAccount->SetLogonState(LS_Granted);
                peAccount->InsertStatus(0);
                peAccount->RemoveStatus(1);
            }

            // Account account items are disabled
            peAccount->SetEnabled(false);
        }
    }
    pvChildren->Release();

    FxLogUserOn(pla);

    // Set frame status
    SetStatus(LoadResString(IDS_LOGGINGON));

#else

    // Set keyfocus back to frame so it isn't pushed anywhere when controls are removed.
    // This will also cause a remove of the password panel from the current account
    SetKeyFocus();
	pla->SetSelected(false);

    // Disable status so that it can't be clicked on anymore
    pla->DisableStatus(0);
    pla->DisableStatus(1);

    //pla->RemoveCredPanel();

    // Clear list of logon accounts except the one logging on
    DirectUI::Value* pvChildren;
    auto peList = _peAccountList->GetChildren(&pvChildren);
    if (peList)
    {
        LogonAccount* peAccount;
        for (UINT i = 0; i < peList->GetSize(); i++)
        {
            peAccount = (LogonAccount*)peList->GetItem(i);

            if (peAccount != pla)
            {
                peAccount->SetLayoutPos(DirectUI::LP_None);
                peAccount->SetLogonState(LS_Denied);
            }
            else
            {
                peAccount->SetLogonState(LS_Granted);
                peAccount->InsertStatus(0);
                peAccount->RemoveStatus(1);
            }

            // Account account items are disabled
            peAccount->SetEnabled(false);
        }
    }
    pvChildren->Release();

    // Hide option buttons
    HidePowerButton();
    HideUndockButton();

    // Set frame status
    SetStatus(LoadResString(IDS_LOGGINGON),false);

    _peViewer->RemoveListener(this);
    _peAccountList->RemoveListener(this);

#endif

    EndDefer(cookie);

    return S_OK;
}

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

HRESULT LogonFrame::OnPower()
{
    //TODO

	/*static auto ClassicShutdownDll = LoadLibrary(TEXT("ClassicShutdown.dll"));

	if (!ClassicShutdownDll)
		return S_OK;

	HRESULT(*DisplayShutdownDialog)(HWND,SHUTDOWNSTYLE,SHUTDOWNTYPE,PSHUTDOWNOPTIONS) = (decltype(DisplayShutdownDialog))(GetProcAddress(ClassicShutdownDll, "DisplayShutdownDialog"));
	if (!DisplayShutdownDialog)
		return E_FAIL;

	DisplayShutdownDialog(0,SDS_WINXP,SHTDN_NONE,NULL);*/

    return S_OK;
}

HRESULT LogonFrame::OnUndock()
{
    //TODO
    return S_OK;
}


UINT_PTR g_puTimerId = 0;
UINT_PTR g_puFlagTimerId = 0;
UINT_PTR g_puUpdateTimeId = 0;

DWORD sTimerCount = 0;

HRESULT LogonFrame::OnTreeReady(DirectUI::DUIXmlParser* pParser)
{
    HRESULT hr;

    // Cache
    _pParser = pParser;

    // Cache important descendents
    _peAccountList = (DirectUI::Selector*)FindDescendent(DirectUI::StrToID(L"accountlist"));
    assert(_peAccountList, "Cannot find account list, check the UI file");
    if (_peAccountList == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peLeftPanel = (Element*)FindDescendent(DirectUI::StrToID(L"leftpanel"));
    assert(_peLeftPanel, "Cannot find left panel, check the UI file");
    if (_peLeftPanel == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peViewer = (DirectUI::ScrollViewer*)FindDescendent(DirectUI::StrToID(L"scroller"));
    assert(_peViewer, "Cannot find scroller list, check the UI file");
    if (_peViewer == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peRightPanel = (DirectUI::Selector*)FindDescendent(DirectUI::StrToID(L"rightpanel"));
    assert(_peRightPanel, "Cannot find account list, check the UI file");
    if (_peRightPanel == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peLogoArea = (DirectUI::Element*)FindDescendent(DirectUI::StrToID(L"logoarea"));
    assert(_peLogoArea, "Cannot find logo area, check the UI file");
    if (_peLogoArea == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peDateTimeArea = (DirectUI::Element*)FindDescendent(DirectUI::StrToID(L"dateandtime"));
    assert(_peDateTimeArea, "Cannot find date and time area, check the UI file");
    if (_peDateTimeArea == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

	_peDateArea = (DirectUI::Element*)_peDateTimeArea->FindDescendent(DirectUI::StrToID(L"date"));
	assert(_peDateArea, "Cannot find date area, check the UI file");
	if (_peDateArea == NULL)
	{
		hr = E_OUTOFMEMORY;
		return hr;
	}

	_peTimeArea = (DirectUI::Element*)_peDateTimeArea->FindDescendent(DirectUI::StrToID(L"time"));
	assert(_peTimeArea, "Cannot find time area, check the UI file");
	if (_peTimeArea == NULL)
	{
		hr = E_OUTOFMEMORY;
		return hr;
	}

    _peMsgArea = (DirectUI::Element*)FindDescendent(DirectUI::StrToID(L"msgarea"));
    assert(_peMsgArea, "Cannot find welcome area, check the UI file");
    if (_peMsgArea == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _pbPower = (DirectUI::Button*)FindDescendent(DirectUI::StrToID(L"power"));
    assert(_pbPower, "Cannot find power button, check the UI file");
    if (_pbPower == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _pbUndock = (DirectUI::Button*)FindDescendent(DirectUI::StrToID(L"undock"));
    assert(_pbUndock, "Cannot find undock button, check the UI file");
    if (_pbUndock == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }


    _peHelp = (DirectUI::Button*)FindDescendent(DirectUI::StrToID(L"help"));
    assert(_peHelp, "Cannot find help text, check the UI file");
    if (_peHelp == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    _peOptions = FindDescendent(DirectUI::StrToID(L"options"));
    assert(_peOptions, "Cannot find account list, check the UI file");
    if (_peOptions == NULL)
    {
        hr = E_OUTOFMEMORY;
        return hr;
    }

    // check for small window or low color cases and hide some elements that will look bad then.
    HWND hwnd = _pnhh->GetHWND();
    RECT rcClient;
    Element* pEle;
    HDC hDC = GetDC(hwnd);
    _nColorDepth = GetDeviceCaps(hDC, BITSPIXEL);
    _pParser->GetSheet(L"hotaccountlistss",&_pvHotList);
    _pParser->GetSheet(L"accountlistss",&_pvList);

    ReleaseDC(hwnd, hDC);

    GetClientRect(hwnd, &rcClient);
    if (RECTWIDTH(rcClient) < 780 || _nColorDepth <= 8)
    {
        //no animations
        g_fNoAnimations = true;

        // remove the clouds
        pEle = FindDescendent(DirectUI::StrToID(L"contentcontainer"));
        if (pEle)
        {
            pEle->RemoveLocalValue(ContentProp);
            if (_nColorDepth <= 8)
            {
                pEle->SetBackgroundColor(ORGB(96, 128, 255));
            }
        }

        if (_nColorDepth <= 8)
        {
            pEle = FindDescendent(DirectUI::StrToID(L"product"));
            if (pEle)
            {
                pEle->SetBackgroundColor(ORGB(96, 128, 255));
            }
        }
    }

    _peViewer->AddListener(this);
    _peAccountList->AddListener(this);

    // Setup frame labels
    SetPowerButtonLabel(LoadResString(IDS_POWER));
    SetUndockButtonLabel(LoadResString(IDS_UNDOCK));

    ShowLogoArea();
    HideWelcomeArea();

	if (IsUndockAllowed())
	{
		ShowUndockButton();
	}
	else
	{
		HideUndockButton();
	}

    //DEBUG
    //EnterPostStatusMode();
    g_puTimerId = SetTimer(_hwndNotification, TIMER_REFRESHTIPS, 0, NULL);
	if (settingShouldAnimateFlag == TRUE)
		g_puFlagTimerId = SetTimer(_hwndNotification, TIMER_ANIMATEFLAG, 20, NULL);    // start the flag animation

    g_puUpdateTimeId = SetTimer(_hwndNotification, TIMER_UPDATETIME, 1000, NULL);    // start the time update timer

    return S_OK;


Failure:

    return hr;
}


void KillFlagAnimation()
{
	if (g_plf && g_plf->settingShouldAnimateFlag == TRUE)
	{
		if (sTimerCount > 0 && sTimerCount < TOTAL_FLAG_FRAMES)
		{
			sTimerCount = TOTAL_FLAG_FRAMES + 1;
			if (g_plf != NULL)
			{
				g_plf->NextFlagAnimate(0);
			}
		}
	}
}

void LogonFrame::OnListenedPropertyChanged(Element* peFrom, const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld, DirectUI::Value* pvNew)
{
    UNREFERENCED_PARAMETER(pvOld);
    UNREFERENCED_PARAMETER(pvNew);

    {
        if (((peFrom == _peAccountList) && IsProp(DirectUI::Selector::Selection)) ||
            ((peFrom == _peViewer) && (IsProp(MouseWithin) || IsProp(KeyWithin))))
        {

            bool bHot = false;
            // Move to "hot" account list sheet if mouse or key is within viewer or an item is selected
            if (GetState() == LAS_PreStatus || GetState() == LAS_Logon)
            {
                bHot = _peViewer->GetMouseWithin() || _peAccountList->GetSelection();
            }

            if (!g_fNoAnimations)
            {
                KillFlagAnimation();
                _peAccountList->SetValue(SheetProp, DirectUI::PI_Local, bHot ? _pvHotList : _pvList);
            }
        }
    }
}
