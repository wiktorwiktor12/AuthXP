#include "pch.h"
#include "logonframe.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <WtsApi32.h>
#include <uxtheme.h>

#include "logoninterfaces.h"
#include "userlist.h"
#include "usertileelement.h"
#include "backgroundfetcher.h"
#include "duiutil.h"
#include "errorballoon.h"
#include "logonaccount.h"
#include "logonguids.h"
#include "slpublic.h"
#include "powrprof.h"

using namespace Microsoft::WRL;

/*DirectUI::IClassInfo* CLogonFrame7::Class = nullptr;
CLogonFrame7* CLogonFrame7::_pSingleton = nullptr;

CLogonFrame7::~CLogonFrame7()
{
	if (m_xmlParser)
		m_xmlParser->Destroy();

	if (m_nativeHost)
	{
		m_nativeHost->Destroy();
		DestroyWindow(m_nativeHost->GetHWND());
	}

	UnregisterClassW(L"AUTHUI.DLL: LogonUI MainFrame Timer Window", HINST_THISCOMPONENT);

	DirectUI::HWNDElement::~HWNDElement();
}

DirectUI::IClassInfo* CLogonFrame7::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CLogonFrame7::GetClassInfoPtr()
{
	return Class;
}

HRESULT CLogonFrame7::Create(HWND hParent, bool fDblBuffer, UINT nCreate, Element* pParent, DWORD* pdwDeferCookie,
	DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CLogonFrame7, HWND, bool, UINT>(hParent, fDblBuffer,nCreate, pParent, pdwDeferCookie,ppElement);
}

HRESULT CLogonFrame7::Create(CLogonNativeHWNDHost* host)
{
	_pSingleton = DirectUI::HNew<CLogonFrame7>();
	if (!_pSingleton)
	{
		DestroyWindow(host->GetHWND());
		return E_OUTOFMEMORY;
	}

	DWORD defer;
	HRESULT hr = _pSingleton->_Initialize(host, nullptr, &defer);
	_pSingleton->EndDefer(defer);
	return hr;
}

HRESULT CLogonFrame7::Register()
{
	return DirectUI::ClassInfo<CLogonFrame7, DirectUI::HWNDElement, DirectUI::EmptyCreator<CLogonFrame7>>::RegisterGlobal(HINST_THISCOMPONENT, L"MainFrame", nullptr, 0);
}

CLogonFrame7* CLogonFrame7::GetSingleton()
{
	return _pSingleton;
}

HRESULT CLogonFrame7::CreateStyleParser(DirectUI::DUIXmlParser** outParser)
{
	*outParser = nullptr;

	UINT buttonSet;
	RETURN_IF_FAILED(CBackground::GetButtonSet(&buttonSet));

	DirectUI::DUIXmlParser* parser = nullptr;
	RETURN_IF_FAILED(DirectUI::DUIXmlParser::Create(&parser, 0, 0, 0, 0));

	//parser->SetParseErrorCallback([](const WCHAR* pszError, const WCHAR* pszToken, int dLine, void* pContext) {
	//	MessageBox(nullptr, std::format(L"err: {}; {}; {}\n", pszError, pszToken, dLine).c_str(), L"Error while parsing DirectUI", 0);
	//	DebugBreak();
	//	}, nullptr);

	auto cleaner = wil::scope_exit([&] { parser->Destroy(); });

	RETURN_IF_FAILED(parser->SetXMLFromResource((UIFILE_LOGON + buttonSet),HINST_THISCOMPONENT,nullptr));

	cleaner.release();

	*outParser = parser;
	return S_OK;
}

void CLogonFrame7::OnEvent(DirectUI::Event* pEvent)
{
	if (pEvent->uidType == DirectUI::Button::Click() && pEvent->nStage == DirectUI::GMF_BUBBLED) //non window specific buttons
	{
		if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"ShutDownOptions"))
		{
			_HandleShutdownChoices();
			return DirectUI::HWNDElement::OnEvent(pEvent);
		}
		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"ShutDown"))
		{
			if (m_CurrentWindow == m_SecurityOptions && (GetKeyState(VK_CONTROL) & 0x8000) != 0)
			{
				m_bIsInEmergencyRestartDialog = true;
				_OnEmergencyRestart();
			}
			else
			{
				_ShutdownCommon(_IsInstallUpdatesAndShutdownAllowed() ? 0x20002 : SHUTDOWN_FORCE_SELF);
			}

			return DirectUI::HWNDElement::OnEvent(pEvent);
		}
		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"Accessibility"))
		{
			//there is a proper way to this, but this works just as well, so IDGAF!!
			ShellExecuteW(0, L"open", L"utilman.exe", L"-debug", 0, SW_SHOWNORMAL);
			return DirectUI::HWNDElement::OnEvent(pEvent);
		}
	}

	if (pEvent->uidType == DirectUI::Button::Click() && m_CurrentWindow == m_MessageFrame && pEvent->nStage == DirectUI::GMF_BUBBLED)
	{
		MessageOptionFlag flag;
		if (pEvent->peTarget == m_Ok)
			flag = MessageOptionFlag::Ok;
		else if (pEvent->peTarget == m_Cancel)
			flag = MessageOptionFlag::Cancel;
		else if (pEvent->peTarget == m_No)
			flag = MessageOptionFlag::No;
		else if (pEvent->peTarget == m_Yes)
			flag = MessageOptionFlag::Yes;
		else
			flag = MessageOptionFlag::None;

		if (flag != MessageOptionFlag::None)
		{
			if (m_MessageDisplayResultCompletion.get() != nullptr)
			{
				OnMessageOptionPressed(flag);
			}
			else
			{
				if (!m_bIsInEmergencyRestartDialog)
					m_consoleUIManager->ShowCredentialView();
				else
				{
					if (flag == MessageOptionFlag::Yes || flag == MessageOptionFlag::Ok)
					{
						ConfirmEmergencyShutdown();
					}
					else if (flag == MessageOptionFlag::No || flag == MessageOptionFlag::Cancel)
					{
						OnSecurityOptionSelected(LC::LogonUISecurityOptions_Cancel);
						m_bIsInEmergencyRestartDialog = false;
					}

					DirectUI::HWNDElement::OnEvent(pEvent);
				}
			}
		}
	}

	if (pEvent->uidType == DirectUI::Button::Click() && m_CurrentWindow == m_activeUserList && pEvent->nStage == DirectUI::GMF_BUBBLED)
	{
		if (IsElementOfClass(pEvent->peTarget,L"UserTile"))
		{
			CDUIUserTileElement* tile = static_cast<CDUIUserTileElement*>(pEvent->peTarget);
			if (tile->m_dataSourceCredential.Get())
			{
				ComPtr<IInspectable> inspectable;
				LOG_IF_FAILED(tile->m_dataSourceCredential.As(&inspectable));
				//m_consoleUIManager->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->m_dataSourceUser.Get());
				m_consoleUIManager->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->m_dataSourceUser.Get() ? tile->m_dataSourceUser.Get() : inspectable.Get());
				LOG_HR_MSG(E_FAIL,"PUTTING THING");
				//HRESULT hr = BeginInvoke(m_consoleUIManager->m_Dispatcher.Get(), [=]() -> void
				//{
				//	UNREFERENCED_PARAMETER(this);
				//	thisRef->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->m_dataSourceCredential.Get());
				//});
			}
			else
			{
				LOG_HR_MSG(E_FAIL,"TILE HAD NO CREDENTIAL DATASOURCE");
			}
		}

		if (pEvent->peTarget == m_Cancel && m_currentReason == LC::LogonUIRequestReason_LogonUIChange)
		{
			LOG_HR_IF_NULL_MSG(E_FAIL,m_consoleUIManager.Get(),"m_consoleUIManager IS NULL!!!");
			if (m_consoleUIManager.Get() && m_consoleUIManager->m_requestCredentialsComplete.get())
			{
				ComPtr<LogonViewManager> thisRef = m_consoleUIManager;
				HRESULT hr = BeginInvoke(m_consoleUIManager->m_Dispatcher.Get(), [=]() -> void
				{
					UNREFERENCED_PARAMETER(this);
					thisRef->m_requestCredentialsComplete->Complete(HRESULT_FROM_WIN32(ERROR_CANCELLED));
					thisRef->ClearCredentialStateUIThread();
					thisRef->m_requestCredentialsComplete.reset();
				});

				//m_consoleUIManager->m_requestCredentialsComplete = nullptr;
			}
		}
		else if (pEvent->peTarget == m_SwitchUser)
		{
			if (!GetSystemMetrics(SM_REMOTESESSION))
			{
				ComPtr<LogonViewManager> thisRef = m_consoleUIManager;
				HRESULT hr = BeginInvoke(m_consoleUIManager->m_Dispatcher.Get(), [=]() -> void
				{
					UNREFERENCED_PARAMETER(this);
					if (m_currentReason == LC::LogonUIRequestReason_LogonUIUnlock)
					{
						//RETURN_IF_FAILED(thisRef->m_userSettingManager->put_IsLockScreenAllowed(FALSE)); // 281
						if (SUCCEEDED(thisRef->m_userSettingManager->put_IsLockScreenAllowed(FALSE)))
							WTSDisconnectSession(nullptr, WTS_CURRENT_SESSION, FALSE);
					}
					else if (m_currentReason == LC::LogonUIRequestReason_LogonUILogon)
					{
						//RETURN_IF_FAILED(thisRef->m_credProvDataModel->put_SelectedUserOrV1Credential(nullptr)); // 286
						thisRef->m_credProvDataModel->put_SelectedUserOrV1Credential(nullptr); // 286
					}
				});

			}
		}

		return DirectUI::HWNDElement::OnEvent(pEvent);
	}

	if (pEvent->uidType == DirectUI::Button::Click() && m_CurrentWindow == m_SecurityOptions && m_SecurityOptionsCompletion && pEvent->nStage == DirectUI::GMF_BUBBLED)
	{
		LC::LogonUISecurityOptions options;
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
		else if (pEvent->peTarget->GetID() == DirectUI::StrToID(L"Cancel"))
			options = LC::LogonUISecurityOptions_Cancel;
		else
		{
			return DirectUI::HWNDElement::OnEvent(pEvent);
		}

		if (m_bIsInEmergencyRestartDialog == false)
			OnSecurityOptionSelected(options);
	}

	return DirectUI::HWNDElement::OnEvent(pEvent);
}

bool CLogonFrame7::_ShowBackgroundBitmap()
{
	return GetSystemMetrics(SM_REMOTESESSION) != 0; //7 authui creates a com object of CSystemSettings and calls CSystemSettings::IsLogonWallpaperShown which does this.
}

BOOL SHWindowsPolicy(REFGUID rpolid)
{
	static BOOL(WINAPI* fSHWindowsPolicy)(REFGUID) = decltype(fSHWindowsPolicy)(GetProcAddress(LoadLibrary(L"SHLWAPI.dll"), MAKEINTRESOURCEA(618)));
	return fSHWindowsPolicy(rpolid);
}

bool CLogonFrame7::_IsSwitchUserAllowed()
{
	if ( !GetSystemMetrics(SM_REMOTESESSION) )
	{
		DWORD allowMultipleSessions;
		if (SUCCEEDED(SLGetWindowsInformationDWORD(L"TerminalServices-RemoteConnectionManager-AllowMultipleSessions", &allowMultipleSessions)))
		{
			if ( allowMultipleSessions != 0 )
				return !SHWindowsPolicy(POLID_HideFastUserSwitching);
		}

	}
	return false;
}

void CLogonFrame7::SetBackgroundGraphics()
{
	auto InsideFrameElement = FindDescendent(DirectUI::StrToID(L"InsideFrame"));

	HBITMAP bitmap = nullptr;
	CBackground::GetBackground(&bitmap);

	if (!bitmap)
		return;

	auto graphic = DirectUI::Value::CreateGraphic(bitmap, 4, 0xFFFFFFFF, false, false, false);
	if (!graphic)
	{
		DeleteObject(bitmap);
		return;
	}

	InsideFrameElement->SetValue(DirectUI::Element::BackgroundProp, 1, graphic);
	graphic->Release();
}

struct SecurityOptionsFlags
{
	LC::LogonUISecurityOptions flag;
	const wchar_t* id;
};

SecurityOptionsFlags secOptsFlags[] = { {LC::LogonUISecurityOptions_Lock,L"SecurityLock"},{LC::LogonUISecurityOptions_LogOff,L"SecurityLogOff"},{LC::LogonUISecurityOptions_ChangePassword,L"SecurityChange"},{LC::LogonUISecurityOptions_TaskManager,L"SecurityTaskManager"},{LC::LogonUISecurityOptions_SwitchUser,L"SecuritySwitchUser"}};

void CLogonFrame7::ShowSecurityOptions(LC::LogonUISecurityOptions SecurityOptsFlag, WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>> completion)
{
	m_SecurityOptionsCompletion = wil::make_unique_nothrow<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::ILogonUISecurityOptionsResult>>>(completion);
	if (m_SecurityOptionsCompletion.get() == nullptr)
		return;

	DWORD cookie;
	StartDefer(&cookie);

	DirectUI::Element* InsideWindow = m_Window->FindDescendent(DirectUI::StrToID(L"InsideWindow"));
	InsideWindow->Add(m_SecurityOptions);
	m_SecurityOptions->SetVisible(true);
	//if (IsHighContrastOn())
	//    _SetUIForHighContrast(1,InsideWindow);
	SetBackgroundGraphics();
	_SelectMode(m_SecurityOptions,true);

	//DWORD optsFlag = 32;
	//if ((SecurityOptsFlag & 0x1F0) != 0)
	//{
	//	//SecurityOptsFlag &= 0xFFFFFE0F;
	//	optsFlag = 96;
	//}
	SetOptions(MessageOptionFlag::Cancel | MessageOptionFlag::ShutDownFrame | MessageOptionFlag::Accessibility);
	//if (!_IsSwitchUserAllowed())
	//    SecurityOptsFlag &= ~0x200u;
	bool bPastFirst = false;
	for (int i = 0; i < ARRAYSIZE(secOptsFlags); ++i)
	{
		SecurityOptionsFlags& opt = secOptsFlags[i];
		DirectUI::Element* button = FindDescendent(DirectUI::StrToID(opt.id));
		if ((SecurityOptsFlag & opt.flag) != 0)
		{
			button->SetVisible(true);
			button->SetLayoutPos(-1);
			if (!bPastFirst)
			{
				bPastFirst = true;
				button->SetKeyFocus();
			}
		}
		else
		{
			button->SetVisible(false);
			button->SetLayoutPos(-3);
		}
	}
	if (!bPastFirst)
		m_Cancel->SetKeyFocus();

	if (cookie)
		EndDefer(cookie);
}

HRESULT CLogonFrame7::OnSecurityOptionSelected(LC::LogonUISecurityOptions SecurityOpt)
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

HRESULT CLogonFrame7::ConfirmEmergencyShutdown()
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

void CLogonFrame7::ShowStatusMessage(const wchar_t* message)
{
	return _DisplayStatusMessage(message, true);
}

HRESULT CLogonFrame7::_Initialize(CLogonNativeHWNDHost* Host, DirectUI::Element* pParent, DWORD* DeferCookie)
{
	RETURN_IF_FAILED(CoCreateInstance(CLSID_AuthUIShutdownChoices, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_shutdownChoices)));

	DWORD dwChoiceMask = 0x400781 | 0x20006 | 0x200050;
	//dwChoiceMask &= ~0x200000;
	m_shutdownChoices->SetChoiceMask(dwChoiceMask);
	m_shutdownChoices->SetShowBadChoices(TRUE);

    m_nativeHost = Host;
    RETURN_IF_FAILED(DirectUI::HWNDElement::Initialize(m_nativeHost->GetHWND(),1,4,nullptr, DeferCookie));

    RETURN_IF_FAILED(CreateStyleParser(&m_xmlParser));

    DWORD defer;
    StartDefer(&defer);

    auto deferCleaner = wil::scope_exit([&] { EndDefer(defer); });

    DirectUI::Layout* fillLayout;
    RETURN_IF_FAILED(DirectUI::FillLayout::Create(&fillLayout));
    auto layoutCleaner = wil::scope_exit([&] { fillLayout->Destroy(); });

    RETURN_IF_FAILED(SetLayout(fillLayout));

    DirectUI::Element* out;
    RETURN_IF_FAILED(m_xmlParser->CreateElement(L"MainFrame",this,0,0,&out));

    m_Status = FindDescendent(DirectUI::StrToID(L"Status"));
    m_StatusText = FindDescendent(DirectUI::StrToID(L"StatusText"));
    m_WaitAnimation = FindDescendent(DirectUI::StrToID(L"WaitAnimation"));
    m_Locked = FindDescendent(DirectUI::StrToID(L"Locked"));
    m_Window = FindDescendent(DirectUI::StrToID(L"Window"));
    m_MessageFrame = m_Window->FindDescendent(DirectUI::StrToID(L"MessageFrame"));
    m_FullMessageFrame = m_Window->FindDescendent(DirectUI::StrToID(L"FullMessageFrame"));
    m_ShortMessageFrame = m_Window->FindDescendent(DirectUI::StrToID(L"ShortMessageFrame"));
    m_ConnectMessageFrame = m_Window->FindDescendent(DirectUI::StrToID(L"ConnectMessageFrame"));
    m_SecurityOptions = m_Window->FindDescendent(DirectUI::StrToID(L"SecurityOptions"));
    m_SwitchUser = m_Window->FindDescendent(DirectUI::StrToID(L"SwitchUser"));
    m_OtherTiles = m_Window->FindDescendent(DirectUI::StrToID(L"OtherTiles"));
    m_Ok = m_Window->FindDescendent(DirectUI::StrToID(L"Ok"));
    m_Yes = m_Window->FindDescendent(DirectUI::StrToID(L"Yes"));
    m_No = m_Window->FindDescendent(DirectUI::StrToID(L"No"));
    m_Cancel = m_Window->FindDescendent(DirectUI::StrToID(L"Cancel"));
    m_Options = FindDescendent(DirectUI::StrToID(L"Options"));
    m_ShutDownFrame = m_Options->FindDescendent(DirectUI::StrToID(L"ShutDownFrame"));
    m_ShowPLAP = m_Options->FindDescendent(DirectUI::StrToID(L"ShowPLAP"));
    m_DisconnectPLAP = m_Options->FindDescendent(DirectUI::StrToID(L"DisconnectPLAP"));
    m_Accessibility = m_Options->FindDescendent(DirectUI::StrToID(L"Accessibility"));

    DirectUI::Element* InsideWindow = m_Window->FindDescendent(DirectUI::StrToID(L"InsideWindow"));
    InsideWindow->Remove(m_SecurityOptions);
    SetBackgroundColor(_ShowBackgroundBitmap() != false ? 0xFF000000 : 0xFF7A5F1D);

    SetBackgroundGraphics();
    _SetBrandingGraphic();
    SetVisible(true);
    SetActive(7);

    SetOptions(MessageOptionFlag::None);
    m_nativeHost->Host(this);

    RETURN_IF_FAILED(_InitializeUserLists());

    //todo: handle the other extra stuff like high contrast
    layoutCleaner.release();
    return S_OK;
}

HRESULT CLogonFrame7::_InitializeUserLists()
{
	m_LogonUserList = (UserList*)FindDescendent(DirectUI::StrToID(L"LogonUserList"));
	m_PLAPUserList = (UserList*)FindDescendent(DirectUI::StrToID(L"PLAPUserList"));

	RETURN_IF_FAILED(m_LogonUserList->Configure(m_xmlParser));
	RETURN_IF_FAILED(m_PLAPUserList->Configure(m_xmlParser));

	m_PLAPUserList->m_scenario = m_nativeHost->m_scenario; // no plap scenario here, oh well;
	m_LogonUserList->m_scenario = m_nativeHost->m_scenario;

	m_activeUserList = m_LogonUserList;

	return S_OK;
}

bool CLogonFrame7::_IsInstallUpdatesAndShutdownAllowed()
{
	static bool isInstallUpdatesAndShutdownAllowed = false;

	if (!isInstallUpdatesAndShutdownAllowed)
	{
		ComPtr<IEnumShutdownChoices> iterator;
		if (FAILED(m_shutdownChoices->GetChoiceEnumerator(&iterator)))
			return false;

		DWORD sc;
		while (iterator->Next(1, &sc, nullptr) == S_OK)
		{
			if ( (WORD)sc == 2 && (sc & 0x20000) != 0 )
			{
				isInstallUpdatesAndShutdownAllowed = true;
				break;
			}
		}
	}

	return isInstallUpdatesAndShutdownAllowed;
}

//TODO:
void CLogonFrame7::_SetSoftKeyboardAllowed(bool allowed)
{
}

static HBITMAP BrandingLoadImage(const wchar_t* a1, __int64 a2, UINT a3, int a4, int a5, UINT a6)
{
	static auto fBrandingLoadImage = reinterpret_cast<HBITMAP(__fastcall*)(const wchar_t* a1, __int64 a2, UINT a3, int a4, int a5, UINT a6)>(GetProcAddress(LoadLibrary(L"winbrand.dll"), "BrandingLoadImage"));
	if (fBrandingLoadImage)
		return fBrandingLoadImage(a1, a2, a3, a4, a5, a6);

	return 0;
}

void CLogonFrame7::_SetBrandingGraphic()
{
	auto brandingElement = FindDescendent(DirectUI::StrToID(L"Branding"));
	if (!brandingElement) return;

	const int brandingSizes[3][2] = { {122,350}, {1122,438} ,{2122,525} };

	int residToUse = 122;
	int lastdist = 9999999;
	int DPI = GetDpiForSystem();

	int scalecompare = MulDiv(350, DPI, 96);// 350 * (DPI / 96);
	for (int i = 0; i < 3; ++i)
	{
		auto pair = brandingSizes[i];
		int resid = pair[0];
		int reso = pair[1];

		int dist = abs(reso - scalecompare);
		if (dist < lastdist)
		{
			lastdist = dist;
			residToUse = resid;
		}
	}

	HBITMAP bitmap = BrandingLoadImage(L"Basebrd", 122, 0, 0, 0, 0); //hardcode to 122, seems like brandingloadimage has stuff to handle dpi on its own
	if (!bitmap)
	{
		bitmap = LoadBitmapW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(residToUse));
	}

	auto graphic = DirectUI::Value::CreateGraphic(bitmap, (unsigned char)2, (unsigned int)0xFFFFFFFF, false, false, false);
	if (!graphic)
	{
		bitmap = LoadBitmapW(HINST_THISCOMPONENT, MAKEINTRESOURCEW(residToUse));
		graphic = DirectUI::Value::CreateGraphic(bitmap, (unsigned char)2, (unsigned int)0xFFFFFFFF, false, false, false);
	}
	if (!graphic)
	{
		DeleteObject(bitmap);
		return;
	}

	brandingElement->SetValue(DirectUI::Element::ContentProp, 1, graphic);
	graphic->Release();
}

void CLogonFrame7::SetOptions(MessageOptionFlag optionsFlag)
{
	struct OptionFlags
	{
		MessageOptionFlag flag;
		DirectUI::Element* Option;
	};

	DWORD cookie;
	StartDefer(&cookie);

	bool bAllowSwitchUser = _IsSwitchUserAllowed();

	//OptionFlags opts[] = { {1,m_SwitchUser},{2,m_OtherTiles},{4,m_Ok},{8,m_Yes},{16,m_No},{32,m_Cancel},{64,m_ShutDownFrame},{128,m_ShowPLAP},{256,m_Accessibility},{512,m_DisconnectPLAP}};
	OptionFlags opts[] =
		{
			{MessageOptionFlag::SwitchUser,m_SwitchUser},
			{MessageOptionFlag::OtherTiles,m_OtherTiles},
			{MessageOptionFlag::Ok,m_Ok},
			{MessageOptionFlag::Yes,m_Yes},
			{MessageOptionFlag::No,m_No},
			{MessageOptionFlag::Cancel,m_Cancel},
			{MessageOptionFlag::ShutDownFrame,m_ShutDownFrame},
			{MessageOptionFlag::ShowPLAP,m_ShowPLAP},
			{MessageOptionFlag::Accessibility,m_Accessibility},
			{MessageOptionFlag::DisconnectPLAP,m_DisconnectPLAP}
		};
	for (int i = 0; i < 10; ++i)
	{
		OptionFlags& opt = opts[i];

		if (opt.Option == m_SwitchUser && bAllowSwitchUser == false)
			continue;

		bool Enable = (optionsFlag & opt.flag) != MessageOptionFlag::None;
		opt.Option->SetVisible(Enable);
		opt.Option->SetEnabled(Enable);
	}
	if ((optionsFlag & MessageOptionFlag::ShutDownFrame) != MessageOptionFlag::None && m_ShutDownFrame)
	{
		m_ShutDownFrame->FindDescendent(DirectUI::StrToID(L"ShutDown"))->SetSelected(_IsInstallUpdatesAndShutdownAllowed());

	}
	if (cookie)
		EndDefer(cookie);
}

void CLogonFrame7::_SelectMode(DirectUI::Element* elementToHost, bool isVisible)
{
	DWORD cookie = 0;
	StartDefer(&cookie);

	DirectUI::Element* currentWindow = m_CurrentWindow;
	if (currentWindow)
	{
		currentWindow->SetVisible(false);
		m_CurrentWindow->SetEnabled(false);
	}

	m_CurrentWindow = elementToHost;
	elementToHost->SetVisible(true);
	m_CurrentWindow->SetEnabled(true);

	HWND hwnd = GetHWND();
	bool shouldEnable = false;
	if (isVisible)
	{
		EnableWindow(hwnd, true);
		SetActive(7);

		m_Window->SetVisible(true);
		shouldEnable = true;
	}
	else
	{
		EnableWindow(hwnd, false);
		EndMenu();
		SetActive(0);

		m_Window->SetVisible(false);
		shouldEnable = false;
	}

	m_Window->SetEnabled(shouldEnable);
	_SetSoftKeyboardAllowed(false);

	if (cookie)
		EndDefer(cookie);
}

static bool g_ShowCursor = true;

void CLogonFrame7::_ShowCursor(bool bShow)
{
	if ( bShow != g_ShowCursor )
	{
		g_ShowCursor = bShow;
		if ( GetSystemMetrics(SM_MOUSEPRESENT) )
		{
			if ( !GetSystemMetrics(SM_REMOTESESSION) )
				ShowCursor(bShow);
		}
	}
}

void CLogonFrame7::_DisplayStatusMessage(const wchar_t* message, bool showSpinner)
{
	DWORD cookie;
	StartDefer(&cookie);

	_SelectMode(m_Status, false);
	//_ShowCursor(!showSpinner);
	m_WaitAnimation->SetVisible(showSpinner);

	SetContentAndAcc(m_StatusText, message);
	SetOptions(MessageOptionFlag::None);

	if (cookie)
		EndDefer(cookie);
}

void CLogonFrame7::SwitchToUserList(class UserList* userList)
{
	DWORD cookie;
	StartDefer(&cookie);

	m_activeUserList->HideAllTiles();

	m_activeUserList = userList;

	_SelectMode(userList, true);
	userList->m_bIsActive = true;

	//userList->AddTestTiles();
	userList->_ShowEnumeratedTilesWorker(-1);

	userList->FindAndSetKeyFocus();
	userList->EnableList();

	userList->SetActive(7);

	SetOptions(MessageOptionFlag::Accessibility | MessageOptionFlag::ShutDownFrame);

	//ShowPLAP->SetVisible(userList == LogonUserList);

	userList->SetVisible(true);
	userList->_SetUnzoomedWidth();

	if (cookie)
		EndDefer(cookie);
}

void CLogonFrame7::DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags,
	WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>> completion)
{
	m_MessageDisplayResultCompletion = wil::make_unique_nothrow<WI::AsyncDeferral<WI::CMarshaledInterfaceResult<LC::IMessageDisplayResult>>>(completion);
	if (m_MessageDisplayResultCompletion.get() == nullptr)
		return;

	_DisplayLogonDialog(messageCaptionContent, messageContent, flags);
}

void CLogonFrame7::DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, WORD flags)
{
	_DisplayLogonDialog(messageCaptionContent, messageContent, flags);

}

HRESULT CLogonFrame7::OnMessageOptionPressed(MessageOptionFlag flag)
{
	UINT messageCode;
	switch (flag)
	{
	case MessageOptionFlag::Ok:
		messageCode = 1;
		break;
	case MessageOptionFlag::Cancel:
		messageCode = 2;
		break;
	case MessageOptionFlag::Yes:
		messageCode = 6;
		break;
	case MessageOptionFlag::No:
		messageCode = 7;
		break;

	default:
		RETURN_HR(E_INVALIDARG); // 111
	}

	ComPtr<LC::IMessageDisplayResultFactory> factory;
	RETURN_IF_FAILED(WF::GetActivationFactory(
		Wrappers::HStringReference(RuntimeClass_Windows_Internal_UI_Logon_Controller_MessageDisplayResult).Get(), &factory));

	ComPtr<LC::IMessageDisplayResult> messageResult;
	RETURN_IF_FAILED(factory->CreateMessageDisplayResult(messageCode, &messageResult));

	RETURN_IF_FAILED(m_MessageDisplayResultCompletion->GetResult().Set(messageResult.Get()));

	m_MessageDisplayResultCompletion->Complete(S_OK);
	m_MessageDisplayResultCompletion.reset();

	return S_OK;
}

void CLogonFrame7::ShowLockedScreen()
{
	DWORD cookie;
	StartDefer(&cookie);

	SetBackgroundGraphics();
	_SelectMode(m_Locked, true);
	SetOptions(MessageOptionFlag::Accessibility);

	HWND hwnd = GetHWND();
	EnableWindow(hwnd, true);

	auto LockedMessage = m_Locked->FindDescendent(DirectUI::StrToID(L"LockedMessage"));
	auto LockedSubMessage = m_Locked->FindDescendent(DirectUI::StrToID(L"LockedSubMessage"));

	DWORD resId = m_currentReason == LC::LogonUIRequestReason_LogonUIUnlock ? 12002 : 12007;

	SetContentAndAccFromResources(LockedMessage,resId,resId);
	SetContentAndAcc(LockedSubMessage, L"");
	SetActive(3);

	if (cookie)
		EndDefer(cookie);
}

static bool LoadIconAsContent(DirectUI::Element* elm, const wchar_t* lpIconName)
{
	HICON IconW = LoadIconW(nullptr, lpIconName);
	if (IconW)
	{
		DirectUI::Value* Graphic = DirectUI::Value::CreateGraphic(IconW, 0, 0, 0);
		if (Graphic)
		{
			elm->SetValue(DirectUI::Element::ContentProp, 1, Graphic);
			elm->SetVisible(true);

			Graphic->Release();
			return true;
		}
	}
	return false;
}

void CLogonFrame7::_DisplayLogonDialog(const wchar_t* messageCaptionContent, const wchar_t* messageContent, UINT flags)
{
	LPWSTR iconId;
    int iconFlags = flags & 0xF0;
	if (iconFlags == 16)
	{
		iconId = IDI_ERROR;
	}
	else if (iconFlags == 48)
	{
		iconId = IDI_WARNING;
	}
	else
	{
		iconId = IDI_INFORMATION;
		if (iconFlags != 64)
			iconId = 0;
	}

    DirectUI::Element* ButtonToFocus;

    MessageOptionFlag optsFlags;
	switch (flags & 0xF)
	{
	case 1:
		optsFlags = MessageOptionFlag::Ok | MessageOptionFlag::Cancel;
		if ((flags & 0x100) == 0)
		{
			ButtonToFocus = m_Ok;
			break;
		}
		ButtonToFocus = m_Cancel;
		break;
	case 3:
		optsFlags = MessageOptionFlag::Cancel | MessageOptionFlag::Yes | MessageOptionFlag::No;
		if ((flags & 0x100) != 0)
		{
			ButtonToFocus = m_No;
			break;
		}
		if ((flags & 0x200) == 0)
		{
			ButtonToFocus = m_Yes;
			break;
		}
		ButtonToFocus = m_Cancel;
		break;
	case 4:
		optsFlags = MessageOptionFlag::Yes | MessageOptionFlag::No;
		if ((flags & 0x100) != 0)
		{
			ButtonToFocus = m_No;
			break;
		}
		ButtonToFocus = m_Yes;
		break;
	case 6:
		optsFlags = MessageOptionFlag::Cancel;
		ButtonToFocus = m_Cancel;
		break;
	default:
		optsFlags = MessageOptionFlag::Ok;
		ButtonToFocus = m_Ok;
		break;
	}

    DWORD cookie;
    StartDefer(&cookie);
    _SelectMode(m_MessageFrame,true);

	SetForegroundWindow(m_nativeHost->GetHWND());
    m_ConnectMessageFrame->SetLayoutPos(-3);

    DirectUI::Element* element;

	if (!messageCaptionContent || !*messageCaptionContent)
	{
        m_FullMessageFrame->SetLayoutPos (-3);
        m_ShortMessageFrame->SetLayoutPos( -1);
        m_ShortMessageFrame->SetVisible  ( 1);
		element = m_ShortMessageFrame->FindDescendent(DirectUI::StrToID(L"ShortMessage"));
		if (!messageContent)
		{
			SetOptions(optsFlags);
			ButtonToFocus->SetKeyFocus();
			if (cookie)
				EndDefer(cookie);
			return;
		}

        SetContentAndAcc(element, messageContent);
		DirectUI::Element* ShortIcon = m_ShortMessageFrame->FindDescendent(DirectUI::StrToID(L"ShortIcon"));
		if (iconId == 0)
			iconId = IDI_INFORMATION;
		bool IconAsContent = LoadIconAsContent(ShortIcon, iconId);
		ShortIcon->SetLayoutPos(IconAsContent != 0 ? -1 : -3);

		SetOptions(optsFlags);
		ButtonToFocus->SetKeyFocus();
		if (cookie)
			EndDefer(cookie);

		return;
	}
	if (!messageContent || !*messageContent)
	{
		m_FullMessageFrame->SetLayoutPos(-3);
		m_ShortMessageFrame->SetLayoutPos( -1);
		m_ShortMessageFrame->SetVisible( 1);
		element = m_ShortMessageFrame->FindDescendent(DirectUI::StrToID(L"ShortMessage"));

		SetContentAndAcc(element, messageCaptionContent);
		DirectUI::Element* ShortIcon = m_ShortMessageFrame->FindDescendent(DirectUI::StrToID(L"ShortIcon"));
		if (iconId == 0)
			iconId = IDI_INFORMATION;
		bool IconAsContent = LoadIconAsContent(ShortIcon, iconId);
        ShortIcon->SetLayoutPos(IconAsContent != 0 ? -1 : -3);

		SetOptions(optsFlags);
		ButtonToFocus->SetKeyFocus();
		if (cookie)
			EndDefer(cookie);

		return;
	}
    m_FullMessageFrame->SetLayoutPos(-1);
    m_ShortMessageFrame->SetLayoutPos(-3);
    m_FullMessageFrame->SetVisible(true);

	DirectUI::Element* FullIcon = m_FullMessageFrame->FindDescendent(DirectUI::StrToID(L"FullIcon"));
    int layoutPos;
	if (LoadIconAsContent(FullIcon, iconId))
		layoutPos = -1;
	else
		layoutPos = -3;

    FullIcon->SetLayoutPos(layoutPos);

	DirectUI::Element* MessageCaption = m_FullMessageFrame->FindDescendent(DirectUI::StrToID(L"MessageCaption"));
	SetContentAndAcc(MessageCaption, messageCaptionContent);

	DirectUI::Element* Message = m_FullMessageFrame->FindDescendent(DirectUI::StrToID(L"Message"));
	SetContentAndAcc(Message, messageContent);

	SetOptions(optsFlags);
    ButtonToFocus->SetKeyFocus();
	if (cookie)
		EndDefer(cookie);
}

void CLogonFrame7::_OnEmergencyRestart()
{
	WCHAR caption[64] = {};
	WCHAR content[256] = {};

	if ( LoadStringW(HINST_THISCOMPONENT, 12010, caption, 64) )// Emergency restart
	{
		if ( LoadStringW(HINST_THISCOMPONENT, 12011, content, 256) )// Click OK to immediately restart your computer.  Any un-saved data will be lost.  Use this only as a last resort.
		{
			m_bIsInEmergencyRestartDialog = true;
			_DisplayLogonDialog(caption, content, 257);
		}
	}
}

__int64 __fastcall SHOpenEffectiveToken(DWORD DesiredAccess, int a2, void **a3)
{
	HANDLE CurrentThread; // rax
	__int64 result; // rax
	HANDLE v8; // rax
	HANDLE CurrentProcess; // rax

	*a3 = 0;
	CurrentThread = GetCurrentThread();
	if ( OpenThreadToken(CurrentThread, DesiredAccess, 0, a3) )
		return 0;
	result = ResultFromKnownLastError();
	if ( (int)result >= 0 )
		return result;
	if ( a2 && (DWORD)result == -2147024891 )
	{
		v8 = GetCurrentThread();
		if ( !OpenThreadToken(v8, DesiredAccess, 1, a3) )
		{
			result = ResultFromKnownLastError();
			goto LABEL_7;
		}
		return 0;
	}
	LABEL_7:
	  if ( (DWORD)result != -2147023888 )
	  	return result;
	CurrentProcess = GetCurrentProcess();
	if ( OpenProcessToken(CurrentProcess, DesiredAccess, a3) )
		return 0;
	return ResultFromKnownLastError();
}

DWORD __fastcall SetPrivilegeAttribute(const unsigned __int16 *a1, DWORD a2, _TOKEN_ELEVATION_TYPE*a3)
{
	DWORD LastError; // ebx
	DWORD ReturnLength; // [rsp+30h] [rbp-40h] BYREF
	HANDLE TokenHandle; // [rsp+38h] [rbp-38h] BYREF
	_LUID Luid; // [rsp+40h] [rbp-30h] BYREF
	struct _TOKEN_PRIVILEGES NewState; // [rsp+48h] [rbp-28h] BYREF
	struct _TOKEN_PRIVILEGES PreviousState; // [rsp+58h] [rbp-18h] BYREF

	if ( LookupPrivilegeValueW(0, L"SeShutdownPrivilege", &Luid) && (int)SHOpenEffectiveToken(0x28u, 1, &TokenHandle) >= 0 )
	{
		NewState.Privileges[0].Luid = Luid;
		NewState.PrivilegeCount = 1;
		NewState.Privileges[0].Attributes = a2;
		ReturnLength = 16;
		if ( AdjustTokenPrivileges(TokenHandle, 0, &NewState, 0x10u, &PreviousState, &ReturnLength) && a3 )
			*a3 = (_TOKEN_ELEVATION_TYPE)PreviousState.Privileges[0].Attributes;
		LastError = GetLastError();
		CloseHandle(TokenHandle);
	}
	else
	{
		return GetLastError();
	}
	return LastError;
}

void CLogonFrame7::_HandleShutdownChoices()
{
	HMENU popupMenu = CreatePopupMenu();
	if (!popupMenu) return; //gg

	auto ShutdownOptionsElement = FindDescendent(DirectUI::StrToID(L"ShutDownOptions"));
	if (!ShutdownOptionsElement) return;



	ComPtr<IEnumShutdownChoices> iterator;
	if (FAILED(m_shutdownChoices->GetChoiceEnumerator(&iterator)))
		return;

	CCoSimpleArray<DWORD> choices;

	DWORD sc;
	while (iterator->Next(1, &sc, nullptr) == S_OK)
	{
		choices.InsertAt((DWORD)sc,(size_t)0);
	}

	int offset = 0;
	for (int i = 0; i < choices.GetSize(); ++i)
	{
		DWORD choice = choices[i];

		MENUITEMINFOW mi = { sizeof(mi) };
		{
			WCHAR szChoiceName[200];
			if (SUCCEEDED(m_shutdownChoices->GetChoiceName(choice, TRUE, szChoiceName, ARRAYSIZE(szChoiceName))))
			{
				mi.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
				mi.fType = MFT_STRING;
				mi.fState = MFS_ENABLED;
				mi.wID = choice + 1;
				mi.dwTypeData = szChoiceName;
				mi.cch = static_cast<UINT>(wcslen(szChoiceName));
				InsertMenuItemW(popupMenu, i+offset, TRUE, &mi);

				if (i == 0)
				{
					MENUITEMINFOW mi = { sizeof(mi) };
					mi.fMask = 0x103;
					mi.fType = MFT_SEPARATOR;
					InsertMenuItemW(popupMenu, i+1, TRUE, &mi);
					offset++;
				}
			}
		}

	}

	int x = 0;
	int y = 0;

	DirectUI::Value* val;
	for (auto elm = ShutdownOptionsElement; elm; elm = elm->GetParent())
	{
		const POINT* location = elm->GetLocation(&val);
		if (location)
		{
			x += location->x;
			y += location->y;
		}
		val->Release();
	}

	HWND hwnd = m_nativeHost->GetHWND();

	if ( (GetWindowLongW(hwnd, -20) & 0x400000) == 0 )
		x += ShutdownOptionsElement->GetWidth();

	BOOL returnVal = TrackPopupMenuEx(popupMenu, 394, x, y, hwnd, nullptr);
	DWORD choice = -1;
	if (returnVal)
		choice = returnVal - 1;

	DestroyMenu(popupMenu);

	_ShutdownCommon(choice);
}

void CLogonFrame7::_ShutdownCommon(DWORD choice)
{
	if ((choice & 6) != 0)
	{
		_TOKEN_ELEVATION_TYPE v12;
		if (!SetPrivilegeAttribute(0,2,&v12))
		{
			InitiateShutdownW(0,0,0,choice,0);
		}
	}
	else if ((choice & 0x50) != 0)
	{
		SetSuspendState((choice & 0x40) != 0, 0, 0);
	}
	else
	{
		MessageBoxW(0,L"I did not implement this edge case because i did not think its needed! MAKE AN ISSUE",L"Oops!",0);
		//WinStationDisconnect();
	}
}*/

#define ICC_WINLOGON_REINIT    0x80000000
void PokeComCtl32()
{
	INITCOMMONCONTROLSEX iccex = { sizeof(INITCOMMONCONTROLSEX), ICC_WINLOGON_REINIT | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES };
	InitCommonControlsEx(&iccex);
}

#define UNLEN       256                 // Maximum user name length
WCHAR szLastSelectedName[UNLEN + sizeof('\0')] = { L'\0' };

HRESULT BuildUserListFromGina(LogonFrame* plf, OUT LogonAccount** ppAccount)
{
	//TODO: IMPL

    if (ppAccount)
    {
        *ppAccount = NULL;
    }




    // User logon list is now available
    plf->SetUserListAvailable(true);


    return S_OK;
}

//TODO: move to a member function, seriously? why did they ever make it not?
HRESULT BuildAccountList(LogonFrame* plf, OUT LogonAccount** ppla)
{
	HRESULT hr;

	if (ppla)
	{
		*ppla = NULL;
	}

	hr = BuildUserListFromGina(plf, ppla);
	if (SUCCEEDED(hr))
	{
		g_plf->SetUserListAvailable(TRUE);
	}
#ifdef GADGET_ENABLE_GDIPLUS
	plf->FxStartup();
#endif

	return hr;
}

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
    LogonAccount* plaAutoSelect = NULL;

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

        // Create password panel
        //Element* pePwdPanel;
        //_pParser->CreateElement(L"passwordpanel", NULL, 0,0, &pePwdPanel);
        //assert(pePwdPanel, "Can't create password panel");
        //
        //// Cache password panel edit control
        //DirectUI::Edit* pePwdEdit = (DirectUI::Edit*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"password"));
        //assert(pePwdPanel, "Can't create password edit control");
        //
        //// Cache password panel info button
        //DirectUI::Button* pbPwdInfo = (DirectUI::Button*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"info"));
        //assert(pePwdPanel, "Can't create password info button");
        //
        //// Cache password panel keyboard element
        //Element* peKbdIcon = (DirectUI::Button*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"keyboard"));
        //assert(pePwdPanel, "Can't create password keyboard icon");
        //
        //LogonAccount::InitCredPanel(pePwdPanel, pePwdEdit, pbPwdInfo, peKbdIcon);
    }

    //TODO:
    //BuildAccountList(this, &plaAutoSelect);

    DirectUI::StyledScrollViewer* secOpts = (DirectUI::StyledScrollViewer*)FindDescendent(DirectUI::StrToID(L"SecurityOptions"));
    secOpts->SetLayoutPos(DirectUI::LP_None);

	FindDescendent(DirectUI::StrToID(L"scroller"))->SetVisible(TRUE);
	FindDescendent(DirectUI::StrToID(L"scroller"))->SetLayoutPos(DirectUI::BLP_Left);
	FindDescendent(DirectUI::StrToID(L"accountlist"))->SetLayoutPos(DirectUI::LP_Auto);

    if (szLastSelectedName[0] != L'\0')
    {
        LogonAccount* pAccount;
        pAccount = InternalFindNamedUser(szLastSelectedName);
        if (pAccount != NULL)
        {
            plaAutoSelect = pAccount;
        }
        szLastSelectedName[0] = L'\0';
    }

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

    if (!plaAutoSelect)
    {
        SetKeyFocus();
    }

    EndDefer(cookie);

    // Set state
    SetState(LAS_Logon);

    // Set auto-select item, if exists
    //if (plaAutoSelect)
    //{
    //    plaAutoSelect->SetKeyFocus();
    //    _peAccountList->SetSelection(plaAutoSelect);
    //}

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
	WCHAR content[256];

	int StringW = LoadStringW(HINST_THISCOMPONENT, IDS_UNLOCK, content, 256);
	if (StringW <= 0)
		return HRESULTFromLastErrorError();

	SetStatus(content);
	HideAccountPanel();
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
#ifndef ANIMATE_FLAG
    UNREFERENCED_PARAMETER(dwFrame);
#else
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
            pv = pe->GetValue(Element::ContentProp, PI_Local,0);
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
#endif
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

#ifdef ANIMATE_FLAG
        hDC = GetDC(hParent);
        _hdcAnimation = CreateCompatibleDC(hDC);
        if (_hdcAnimation)
        {
            _hbmpFlags = (HBITMAP)LoadImage(gHinstance, MAKEINTRESOURCE(IDB_FLAGSTRIP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
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
#endif
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
			HideAccountPanel();
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

//TODO: IMPL
HRESULT LogonFrame::AddAccountFromTile(const Microsoft::WRL::ComPtr<LCPD::ICredential>& tileData, const Microsoft::WRL::ComPtr<LCPD::IUser>& user, OUT LogonAccount** ppla)
{
	//*ppla = NULL;
	//return E_NOTIMPL;
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

    if (ppla)
        *ppla = pla;

    return S_OK;


Failure:

    return hr;
}

//HRESULT LogonFrame::AddAccount(LPCWSTR pszPicture, BOOL fPicRes, LPCWSTR pszName, LPCWSTR pszUsername, LPCWSTR pszHint,
//    BOOL fPwdNeeded, BOOL fLoggedOn, LogonAccount** ppla)
//{
//    HRESULT hr;
//    LogonAccount* pla = NULL;
//
//    if (!_pParser)
//    {
//        hr = E_FAIL;
//        goto Failure;
//    }
//
//    // Build up an account and insert into selection list
//    hr = _pParser->CreateElement(L"accountitem", NULL, 0,0,(Element**)&pla);
//    if (FAILED(hr))
//        goto Failure;
//
//    pla->_pParser = _pParser;
//    hr = pla->OnTreeReady(pszPicture, fPicRes, pszName, pszUsername, pszHint, fPwdNeeded, fLoggedOn, GetHInstance());
//    if (FAILED(hr))
//        goto Failure;
//
//
//    hr = _peAccountList->Add(pla);
//    if (FAILED(hr))
//        goto Failure;
//
//    if (pla)
//    {
//        SetElementAccessability(pla, true, ROLE_SYSTEM_LISTITEM, pszUsername);
//    }
//
//    if (_nColorDepth <= 8)
//    {
//        pla->SetBackgroundColor(ORGB(96, 128, 255));
//
//        Element* pEle;
//        pEle = pla->FindDescendent(DirectUI::StrToID(L"userpane"));
//        if (pEle)
//        {
//            pEle->SetBorderColor(ORGB(96, 128, 255));
//        }
//    }
//
//    if (ppla)
//        *ppla = pla;
//
//    return S_OK;
//
//
//Failure:
//
//    return hr;
//}
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
    				//m_consoleUIManager->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->m_dataSourceUser.Get());
    				m_consoleUIManager->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->_pUser.Get() ? tile->_pUser.Get() : inspectable.Get());
    				LOG_HR_MSG(E_FAIL,"PUTTING THING");
    				//HRESULT hr = BeginInvoke(m_consoleUIManager->m_Dispatcher.Get(), [=]() -> void
    				//{
    				//	UNREFERENCED_PARAMETER(this);
    				//	thisRef->m_credProvDataModel->put_SelectedUserOrV1Credential(tile->m_dataSourceCredential.Get());
    				//});
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


#define TIMER_REFRESHTIPS 1014
#define TIMER_ANIMATEFLAG 1015
#define TOTAL_FLAG_FRAMES (FLAG_ANIMATION_COUNT * MAX_FLAG_FRAMES)

UINT_PTR g_puTimerId = 0;
UINT_PTR g_puFlagTimerId = 0;

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
#ifdef ANIMATE_FLAG
    g_puFlagTimerId = SetTimer(_hwndNotification, TIMER_ANIMATEFLAG, 20, NULL);    // start the flag animation
#endif


    return S_OK;


Failure:

    return hr;
}


void KillFlagAnimation()
{
#ifdef ANIMATE_FLAG
    if (sTimerCount > 0 && sTimerCount < TOTAL_FLAG_FRAMES)
    {
        sTimerCount = TOTAL_FLAG_FRAMES + 1;
        if (g_plf != NULL)
        {
            g_plf->NextFlagAnimate(0);
        }
    }
#endif
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
