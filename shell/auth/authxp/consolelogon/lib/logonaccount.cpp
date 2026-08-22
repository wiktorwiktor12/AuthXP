#include "pch.h"
#include "logonaccount.h"

#include <cassert>

#include "advisablebutton.h"
#include "combobox.h"
#include "errorballoon.h"
#include "logonframe.h"
#include "duiutil.h"
#include "labeledcheckbox.h"
#include "restrictededit.h"

void LogonAccount::SetKeyboardIcon(HICON hIcon)
{
    HICON hIconCopy = NULL;

    if (hIcon)
    {
        hIconCopy = CopyIcon(hIcon);
    }

    if (_peKbdIcon && hIconCopy)
    {
        DirectUI::Value* pvIcon = DirectUI::Value::CreateGraphic(hIconCopy,false,false,false);
        _peKbdIcon->SetValue(Element::ContentProp, DirectUI::PI_Local, pvIcon);  // Element takes owners
        _peKbdIcon->SetPadding(0, 5, 0, 7);
        pvIcon->Release();
    }
}

////////////////////////////////////////////////////////
// Property definitions

////////////////////////////////////////////////////////
// ClassInfo (must appear after property definitions)

// LogonState property
static int vvLogonState[] = { DSV_Int, -1 };
static DirectUI::PropertyInfoData dataimpLogonStateProp;
static DirectUI::PropertyInfo impLogonStateProp = { L"LogonState", DirectUI::PF_Normal, 0, vvLogonState, NULL, DirectUI::Value::GetIntZero, &dataimpLogonStateProp };
DirectUI::PropertyInfo* LogonAccount::LogonStateProp = &impLogonStateProp;

////////////////////////////////////////////////////////
// ClassInfo (must appear after property definitions)

// Class properties
static DirectUI::PropertyInfo* _aPI[] = {
                                LogonAccount::LogonStateProp,
};

// Define class info with type and base type, set static class pointer
DirectUI::IClassInfo* LogonAccount::Class = NULL;
HRESULT LogonAccount::Register()
{
    return DirectUI::ClassInfo<LogonAccount, Button>::Register(L"LogonAccount", _aPI, ARRAYSIZE(_aPI));
}


LogonAccount::~LogonAccount()
{
    // Free resources
    if (_pvUsername)
    {
        _pvUsername->Release();
        _pvUsername = NULL;
    }

    if (_pvHint)
    {
        _pvHint->Release();
        _pvHint = NULL;
    }

    // TODO: Account destruction cleanup
}

HRESULT LogonAccount::Initialize(Element* pParent, DWORD* pdwDeferCookie)
{
    // Zero-init members
    _pbStatus[0] = NULL;
    _pbStatus[1] = NULL;
    _pvUsername = NULL;
    _pvHint = NULL;
    _fPwdNeeded = (BOOL)-1; // uninitialized
    _fLoggedOn = FALSE;
    _fHasPwdPanel = FALSE;

    // Do base class initialization
    HRESULT hr = Button::Initialize(DirectUI::AEF_MouseAndKeyboard,pParent,pdwDeferCookie);
    if (FAILED(hr))
        goto Failure;

    // Initialize

    // TODO: Additional LogonAccount initialization code here

    return S_OK;


Failure:

    return hr;
}

ATOM LogonAccount::idPwdGo = NULL;
ATOM LogonAccount::idPwdInfo = NULL;
//DirectUI::Element* LogonAccount::_pePwdPanel = NULL;
//DirectUI::Edit* LogonAccount::_pePwdEdit = NULL;
//DirectUI::Button* LogonAccount::_pbPwdInfo = NULL;
//DirectUI::Element* LogonAccount::_peKbdIcon = NULL;
LogonAccount* LogonAccount::_peCandidate = NULL;

HRESULT LogonAccount::Create(Element* pParent, DWORD* pdwDeferCookie,Element** ppElement)
{
    *ppElement = NULL;

    LogonAccount* pla = DirectUI::HNew<LogonAccount>();
    if (!pla)
        return E_OUTOFMEMORY;

    HRESULT hr = pla->Initialize(pParent,pdwDeferCookie);
    if (FAILED(hr))
    {
        pla->Destroy();
        return hr;
    }

    *ppElement = pla;

    return S_OK;
}

void LogonAccount::OnEvent(DirectUI::Event* pEvent)
{
    if (pEvent->nStage == DirectUI::GMF_DIRECT)  // Direct events
    {
        // Watch for click events initiated by LogonAccounts only
        // if we are not logging someone on
        if (pEvent->uidType == Button::Click)
        {
            if (pEvent->peTarget == this)
            {
                if (IsPasswordBlank())
                {
                    // No password needed, attempt logon
                    OnAuthenticateUser();
                }

                pEvent->fHandled = true;
                return;
            }
        }
    }
    else if (pEvent->nStage == DirectUI::GMF_BUBBLED)  // Bubbled events
    {
        if (pEvent->uidType == Button::Click)
        {
            if (idPwdGo && (pEvent->peTarget->GetID() == idPwdGo))
            {
                // Attempt logon
                OnAuthenticateUser();
                pEvent->fHandled = true;
                return;
            }
            else if (idPwdInfo && (pEvent->peTarget->GetID() == idPwdInfo))
            {
                // Retrieve hint
                OnHintSelect();
                pEvent->fHandled = true;
                return;
            }
            else if (pEvent->peTarget == _pbStatus[0])
            {
                // Retrieve status info
                OnStatusSelect(0);
                pEvent->fHandled = true;
                return;
            }
            else if (pEvent->peTarget == _pbStatus[1])
            {
                // Retrieve status info
                OnStatusSelect(1);
                pEvent->fHandled = true;
                return;
            }
        }
        else if (pEvent->uidType == DirectUI::Edit::Enter)
        {
            if (_pePwdEdit && pEvent->peTarget == _pePwdEdit)
            {
                // Attempt logon
                OnAuthenticateUser();
                pEvent->fHandled = true;
                return;
            }
        }
    }
    Button::OnEvent(pEvent);
}

void LogonAccount::OnInput(DirectUI::InputEvent* pEvent)
{
    DirectUI::KeyboardEvent* pke = (DirectUI::KeyboardEvent*)pEvent;

    if (pke->nDevice == DirectUI::GINPUT_KEYBOARD && pke->nCode == DirectUI::GKEY_DOWN)
    {
        g_pErrorBalloon.HideToolTip();
    }

    Button::OnInput(pEvent);
}

void LogonAccount::OnPropertyChanged(const DirectUI::PropertyInfo* ppi, int iIndex, DirectUI::Value* pvOld,
    DirectUI::Value* pvNew)
{
#ifdef GADGET_ENABLE_GDIPLUS
    // MouseWithin must be before Selected
    if (IsProp(MouseWithin))
    {
        if (pvNew->GetBool())
            FxMouseWithin(fdIn);
        else
            FxMouseWithin(fdOut);
    }
#endif

    if (IsProp(Selected))
    {
        if (pvNew->GetBool())
        {
            InsertCredPanel();

        }
        else
            RemoveCredPanel();
    }
    Button::OnPropertyChanged(ppi, iIndex, pvOld, pvNew);
}

void LogonAccount::OnAuthenticatedUser()
{
    _peCandidate = this;
    g_plf->OnLogUserOn(this);
    g_plf->EnterPostStatusMode();
}

BOOL LogonAccount::OnAuthenticateUser(LPCWSTR pszInPassword)
{
    HRESULT hr;
    // Logon requested on this account
    LPCWSTR pszPassword = L"";
    DirectUI::Value* pv = NULL;

    //ILogonUser* pobjUser;
    VARIANT_BOOL vbLogonSucceeded = VARIANT_TRUE;

    //WCHAR* pszUsername = _pvUsername->GetString();
    //
    //if (pszUsername)
    //{
    //    if (SUCCEEDED(hr = GetLogonUserByLogonName(pszUsername, &pobjUser)))
    //    {
    //        if (!IsPasswordBlank())
    //        {
    //            if (pszInPassword)
    //            {
    //                pszPassword = pszInPassword;
    //            }
    //            else
    //            {
    //                if (_pePwdEdit)
    //                {
    //                    pszPassword = _pePwdEdit->GetContentString(&pv);
    //
    //                    if (!pszPassword)
    //                        pszPassword = L"";
    //
    //                    if (pv)
    //                    {
    //                        pv->Release();
    //                    }
    //                }
    //            }
    //
    //            BSTR bstr = SysAllocString(pszPassword);
    //            pobjUser->logon(bstr, &vbLogonSucceeded);
    //            SysFreeString(bstr);
    //        }
    //        else
    //        {
    //            pobjUser->logon(L"", &vbLogonSucceeded);
    //        }
    //        pobjUser->Release();
    //    }
    //}

    if (vbLogonSucceeded == VARIANT_TRUE)
    {
        OnAuthenticatedUser();
    }
    else
    {
        if (pszInPassword == NULL)
        {
            ShowPasswordIncorrectMessage();
        }
    }

    return (vbLogonSucceeded == VARIANT_TRUE);
}

void CalcBalloonTargetLocation(HWND hwndParent, DirectUI::Element* pe, POINT* ppt)
{
    DirectUI::Value* pv;
    BOOL fIsRTL = (GetWindowLong(hwndParent, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) != 0;

    assert(pe);
    assert(ppt);

    // get the position of the link so we can target the balloon tip correctly
    POINT pt = { 0,0 };

    const SIZE* psize = pe->GetExtent(&pv);
    pt.y += psize->cy / 2;

    if (psize->cx < 100)
    {
        pt.x += psize->cx / 2;
    }
    else
    {
        if (fIsRTL)
        {
            pt.x = (pt.x + psize->cx) - 50;
        }
        else
        {
            pt.x += 50;
        }
    }

    pv->Release();

    while (pe)
    {
        const POINT* ppt = pe->GetLocation(&pv);
        pt.x += ppt->x;

        pt.y += ppt->y;
        pv->Release();
        pe = pe->GetParent();
    }

    *ppt = pt;
}

void LogonAccount::OnHintSelect()
{
    WCHAR szTitle[128];

    assert(_pbPwdInfo);

    // get the position of the link so we can target the balloon tip correctly
    POINT pt = { 0,0 };
    CalcBalloonTargetLocation(g_plf->GetNativeHost()->GetHWND(), _pbPwdInfo, &pt);

    LoadStringW(g_plf->GetHInstance(), IDS_PASSWORDHINTTITLE, szTitle, _ARRAYSIZE(szTitle));
    g_pErrorBalloon.ShowToolTip(GetModuleHandleW(NULL), g_plf->GetHWND(), &pt, (LPWSTR)_pvHint->GetString(), szTitle, TTI_INFO, EB_WARNINGCENTERED, 10000);

    SetElementAccessability(_pePwdEdit, true, ROLE_SYSTEM_STATICTEXT, _pvHint->GetString());

    _pePwdEdit->SetKeyFocus();
}

void LogonAccount::OnStatusSelect(UINT nLine)
{
    if (nLine == LASS_Email)
    {
        UnreadMailTip();
    }
    else if (nLine == LASS_LoggedOn)
    {
        AppRunningTip();
    }
}

#define GRAPHIC_NoBlend                     ((BYTE)0)
#define GRAPHIC_AlphaConst                  ((BYTE)1)
#define GRAPHIC_AlphaConstPerPix            ((BYTE)2)
#define GRAPHIC_TransColor                  ((BYTE)3)
#define GRAPHIC_Stretch                     ((BYTE)4)
#define GRAPHIC_NineGrid                    ((BYTE)5)
#define GRAPHIC_NineGridTransColor          ((BYTE)6)
#define GRAPHIC_NineGridAlphaConstPerPix    ((BYTE)7)

HRESULT LogonAccount::OnTreeReady(LPCWSTR pszPicture, BOOL fPicRes, LPCWSTR pszName, LPCWSTR pszUsername,
    LPCWSTR pszHint, BOOL fPwdNeeded, BOOL fLoggedOn, HINSTANCE hInst)
{
    HRESULT hr;
    Element* pePicture = NULL;
    Element* peName = NULL;
    DirectUI::Value* pv = NULL;

    UNREFERENCED_PARAMETER(fPwdNeeded);

    DWORD cookie;
    StartDefer(&cookie);

    // Cache important descendents
    _pbStatus[0] = (Button*)FindDescendent(DirectUI::StrToID(L"status0"));
    assert(_pbStatus[0], "Cannot find account list, check the UI file");
    if (_pbStatus[0] == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Failure;
    }

    _pbStatus[1] = (Button*)FindDescendent(DirectUI::StrToID(L"status1"));
    assert(_pbStatus[1], "Cannot find account list, check the UI file");
    if (_pbStatus[1] == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Failure;
    }

    // Locate descendents and populate
    pePicture = FindDescendent(DirectUI::StrToID(L"picture"));
    assert(pePicture, "Cannot find account list, check the UI file");
    if (pePicture == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Failure;
    }

    // CreateGraphic handles NULL bitmaps
    pv = DirectUI::Value::CreateGraphic(pszPicture, GRAPHIC_NoBlend, 0, 0, 0, (fPicRes) ? hInst : 0,false,false);
    if (pv)
    {
        // Our preferred size is 1/2 inch (36pt) square.
        USHORT cx = (USHORT)LogonFrame::PointToPixel(36);
        USHORT cy = cx;

        DirectUI::Graphic* pg = pv->GetGraphic();

        // If it's not square, scale the smaller dimension
        // to maintain the aspect ratio.
        if (pg->cx > pg->cy)
        {
            cy = (USHORT)MulDiv(cx, pg->cy, pg->cx);
        }
        else if (pg->cy > pg->cx)
        {
            cx = (USHORT)MulDiv(cy, pg->cx, pg->cy);
        }

        // Did anything change?
        if (cx != pg->cx || cy != pg->cy)
        {
            // Reload the graphic
            pv->Release();
            pv = DirectUI::Value::CreateGraphic(pszPicture, GRAPHIC_NoBlend, 0, cx, cy, (fPicRes) ? hInst : 0,false,false);
        }
    }
    if (!pv)
    {
        // if we can't get the picture, use a default one
        pv = DirectUI::Value::CreateGraphic(MAKEINTRESOURCEW(IDB_USER0), GRAPHIC_NoBlend, 0, (USHORT)LogonFrame::PointToPixel(36), (USHORT)LogonFrame::PointToPixel(36), hInst, false, false);
        if (!pv)
        {
            hr = E_OUTOFMEMORY;
            goto Failure;
        }
    }

    hr = pePicture->SetValue(Element::ContentProp, DirectUI::PI_Local, pv);
    if (FAILED(hr))
        goto Failure;

    pv->Release();
    pv = NULL;

    // Name
    peName = FindDescendent(DirectUI::StrToID(L"username"));
    assert(peName, "Cannot find account list, check the UI file");
    if (peName == NULL)
    {
        hr = E_OUTOFMEMORY;
        goto Failure;
    }

    hr = peName->SetContentString(pszName);
    if (FAILED(hr))
        goto Failure;

    // Store members, will be released in destructor
    if (pszUsername)
    {
        _pvUsername = DirectUI::Value::CreateString(pszUsername,0);
        if (!_pvUsername)
        {
            hr = E_OUTOFMEMORY;
            goto Failure;
        }
    }

    if (pszHint)
    {
        _pvHint = DirectUI::Value::CreateString(pszHint,0);
        if (!_pvHint)
        {
            hr = E_OUTOFMEMORY;
            goto Failure;
        }
    }

    _fLoggedOn = fLoggedOn;

    EndDefer(cookie);

    return S_OK;


Failure:

    EndDefer(cookie);

    if (pv)
        pv->Release();

    return hr;
}

HRESULT LogonAccount::CreateCredPanelElements()
{
    Element* pePwdPanel;
    _pParser->CreateElement(L"passwordpanel", NULL, 0, 0, &pePwdPanel);
    auto scopeExit = wil::scope_exit([&]() -> void {pePwdPanel->Destroy(true);});

    assert(pePwdPanel, "Can't create password panel");

	Microsoft::WRL::ComPtr<WFC::IVectorView<LCPD::ICredentialField*>> fields;
	RETURN_IF_FAILED(_tileData->get_Fields(&fields));

	UINT numFields;
	RETURN_IF_FAILED(fields->get_Size(&numFields));

    for (int i = 0; i < numFields; ++i)
    {
    	Microsoft::WRL::ComPtr<LCPD::ICredentialField> field;
    	RETURN_IF_FAILED(fields->GetAt(i, &field));

		DirectUI::Element* fieldElement = nullptr;
        RETURN_IF_FAILED(CreateField(field, &fieldElement));

        pePwdPanel->Add(fieldElement);
    }

    // Cache password panel edit control
    DirectUI::Edit* pePwdEdit = (DirectUI::Edit*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"password"));
    assert(pePwdPanel, "Can't create password edit control");

    // Cache password panel info button
    DirectUI::Button* pbPwdInfo = (DirectUI::Button*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"info"));
    assert(pePwdPanel, "Can't create password info button");

    // Cache password panel keyboard element
    Element* peKbdIcon = (DirectUI::Button*)pePwdPanel->FindDescendent(DirectUI::StrToID(L"keyboard"));
    assert(pePwdPanel, "Can't create password keyboard icon");

    LogonAccount::InitCredPanel(pePwdPanel, pePwdEdit, pbPwdInfo, peKbdIcon);

    scopeExit.release();

    return S_OK;
}

HRESULT LogonAccount::CreateField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
	LCPD::CredentialFieldKind kind;
	RETURN_IF_FAILED(field->get_Kind(&kind));

    switch (kind)
    {
    case LCPD::CredentialFieldKind_CommandLink:
        return _CreateCommandLinkField(field, ppOutElement);

    case LCPD::CredentialFieldKind_EditText:
        return _CreateEditField(field, ppOutElement);

    case LCPD::CredentialFieldKind_CheckBox:
        return _CreateCheckboxField(field, ppOutElement);

    case LCPD::CredentialFieldKind_ComboBox:
        return _CreateComboBoxField(field, ppOutElement);

    case LCPD::CredentialFieldKind_SubmitButton:
        return _CreateSubmitButton(field, ppOutElement);

    default:
        return E_FAIL;
    }
}

HRESULT LogonAccount::_CreateCommandLinkField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
    CAdvisableButton* peCommandLinkControl;
    RETURN_IF_FAILED(_pParser->CreateElement(L"commandlinkcontrol", NULL, 0, 0, (DirectUI::Element**)&peCommandLinkControl));
    auto scopeExit = wil::scope_exit([&]() -> void {peCommandLinkControl->Destroy(true);});

	Microsoft::WRL::Wrappers::HString label;
	RETURN_IF_FAILED(field->get_Label(label.ReleaseAndGetAddressOf()));

    RETURN_IF_FAILED(peCommandLinkControl->SetContentString(label.GetRawBuffer(NULL)));

    RETURN_IF_FAILED(peCommandLinkControl->SetAccessible(true));

    RETURN_IF_FAILED(peCommandLinkControl->SetAccRole(30));

    RETURN_IF_FAILED(peCommandLinkControl->SetAccName(label.GetRawBuffer(NULL)));

    RETURN_IF_FAILED(peCommandLinkControl->SetActive(3));

    *ppOutElement = peCommandLinkControl;

	scopeExit.release();

    return S_OK;
}

HRESULT LogonAccount::_CreateEditField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
    Element* peEditFieldContainer;
    RETURN_IF_FAILED(_pParser->CreateElement(L"editcontrol", NULL, 0, 0, &peEditFieldContainer));
    auto scopeExit = wil::scope_exit([&]() -> void {peEditFieldContainer->Destroy(true);});

	CDUIRestrictedEdit* restrictedEdit = (CDUIRestrictedEdit*)peEditFieldContainer->FindDescendent(DirectUI::StrToID(L"password"));

    restrictedEdit->m_scenario = LCPD::CredProvScenario_Logon;
    restrictedEdit->m_fieldData = field;

	Microsoft::WRL::ComPtr<LCPD::ICredentialEditField> editFieldData;
	RETURN_IF_FAILED(field->QueryInterface(IID_PPV_ARGS(&editFieldData)));

	Microsoft::WRL::Wrappers::HString label;
	RETURN_IF_FAILED(field->get_Label(label.ReleaseAndGetAddressOf()));

	Microsoft::WRL::Wrappers::HString content;
	RETURN_IF_FAILED(editFieldData->get_Content(content.ReleaseAndGetAddressOf()));

    RETURN_IF_FAILED(restrictedEdit->SetAccessible(true));
    RETURN_IF_FAILED(restrictedEdit->SetAccRole(42));
    RETURN_IF_FAILED(restrictedEdit->SetAccName(content.GetRawBuffer(NULL)));

	BOOLEAN bIsPasswordField;
	RETURN_IF_FAILED(editFieldData->get_IsPasswordField(&bIsPasswordField));

    if (bIsPasswordField)
        RETURN_IF_FAILED(restrictedEdit->SetAccValue(label.GetRawBuffer(NULL)));

    StringStringAllocCopy(label.GetRawBuffer(nullptr), &restrictedEdit->m_hintText);

    restrictedEdit->m_maxTextLength = 127;

    if (bIsPasswordField)
        (restrictedEdit->SetEncodedContentString(content.GetRawBuffer(NULL)));
    else
        (restrictedEdit->SetContentString(content.GetRawBuffer(NULL)));

	*ppOutElement = peEditFieldContainer;

    SetFieldInitialVisibility(field, peEditFieldContainer);

    scopeExit.release();

    return S_OK;
}

HRESULT LogonAccount::_CreateCheckboxField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
    CDUILabeledCheckbox* peCheckbox;
    RETURN_IF_FAILED(_pParser->CreateElement(L"checkboxcontrol", NULL, 0, 0, (DirectUI::Element**)&peCheckbox));
    auto scopeExit = wil::scope_exit([&]() -> void {peCheckbox->Destroy(true);});

	Microsoft::WRL::ComPtr<LCPD::ICheckBoxField> checkboxField;
	RETURN_IF_FAILED(field->QueryInterface(IID_PPV_ARGS(&checkboxField)));

	BOOLEAN isChecked = FALSE;
	RETURN_IF_FAILED(checkboxField->get_Checked(&isChecked));

	Microsoft::WRL::Wrappers::HString label;
	RETURN_IF_FAILED(field->get_Label(label.ReleaseAndGetAddressOf()));

    RETURN_IF_FAILED(peCheckbox->Configure(isChecked,label.GetRawBuffer(NULL)));

    RETURN_IF_FAILED(peCheckbox->SetAccName(label.GetRawBuffer(NULL)));

    //peCheckbox->m_index = index;

	//TODO: FIXME
    //peCheckbox->m_owningElement = this;

    scopeExit.release();

    SetFieldInitialVisibility(field,peCheckbox);

	*ppOutElement = peCheckbox;

    return S_OK;
}

HRESULT LogonAccount::_CreateComboBoxField(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
    CDUIComboBox* comboBox;
    RETURN_IF_FAILED(CDUIComboBox::Create(nullptr, 0, (DirectUI::Element**)&comboBox));
    auto scopeExit = wil::scope_exit([&]() -> void {comboBox->Destroy(true);});


	//TODO: FIXME
	//comboBox->m_index = index;
	//comboBox->m_owningElement = this;

	RETURN_IF_FAILED(comboBox->Advise(field.Get()));

	RETURN_IF_FAILED(comboBox->Rebuild());

    SetFieldInitialVisibility(field, comboBox);

    *ppOutElement = comboBox;

    scopeExit.release();

    return S_OK;
}

HRESULT LogonAccount::_CreateSubmitButton(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element** ppOutElement)
{
    return E_FAIL;
}

//TODO: VERIFY
HRESULT LogonAccount::SetFieldInitialVisibility(Microsoft::WRL::ComPtr<LCPD::ICredentialField>& field, DirectUI::Element* fieldElement)
{
	bool isVisible = false;

	LCPD::CredentialFieldKind kind = LCPD::CredentialFieldKind_StaticText;
	if (field.Get() != nullptr)
		RETURN_IF_FAILED(field->get_Kind(&kind));

	BOOLEAN bIsVisibleInDeselectedTile = TRUE;
	BOOLEAN bIsVisibleInSelectedTile = TRUE;
	BOOLEAN isHidden = FALSE;

	if (field.Get() != nullptr)
	{
		RETURN_IF_FAILED(field->get_IsVisibleInDeselectedTile(&bIsVisibleInDeselectedTile));
		RETURN_IF_FAILED(field->get_IsVisibleInSelectedTile(&bIsVisibleInSelectedTile));
		RETURN_IF_FAILED(field->get_IsHidden(&isHidden));
	}

	/*if (kind == LCPD::CredentialFieldKind_CommandLink)
	{
		isVisible = !isHidden && (GetTileZoomed() ? bIsVisibleInSelectedTile : bIsVisibleInDeselectedTile);
	}
	else if (kind == LCPD::CredentialFieldKind_StaticText)
	{
		LCPD::CredentialTextSize size = fieldData->m_size;

		if (fieldData->m_dataSourceCredentialField.Get() != nullptr)
		{
			Microsoft::WRL::ComPtr<LCPD::ICredentialTextField> textField;
			RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->QueryInterface(IID_PPV_ARGS(&textField)));

			RETURN_IF_FAILED(textField->get_TextSize(&size));
		}

		if (GetTileZoomed() && size == LCPD::CredentialTextSize_Large)
		{

			isVisible = true;
		}
		else if (!GetTileZoomed() && size == LCPD::CredentialTextSize_Small)
		{

			isVisible = true;
		}
		else
			isVisible = false;

		isVisible = isVisible && !isHidden && (GetTileZoomed() ? bIsVisibleInSelectedTile : bIsVisibleInDeselectedTile);
	}*/
	isVisible = !isHidden && bIsVisibleInSelectedTile != 0;

	return SetFieldVisibility(fieldElement, isVisible);
}

HRESULT LogonAccount::SetFieldVisibility(DirectUI::Element* fieldElement, bool bIsVisible)
{
	bool wasVisible = fieldElement->GetVisible();

    RETURN_IF_FAILED(fieldElement->SetVisible(bIsVisible));

    if (wasVisible && !bIsVisible)
    RETURN_IF_FAILED(fieldElement->SetLayoutPos(DirectUI::LP_None));

    if (!wasVisible && bIsVisible)
    RETURN_IF_FAILED(fieldElement->SetLayoutPos(DirectUI::LP_Auto));

    return S_OK;
}

HRESULT LogonAccount::InsertCredPanel()
{
    HRESULT hr;

    // If already have it, or no password is available, or logon state is not pending
    if (_fHasPwdPanel || IsPasswordBlank() || (GetLogonState() != LS_Pending))
        goto Done;

    DWORD cookie;
    StartDefer(&cookie);

    CreateCredPanelElements();
    // Add password panel
    hr = FindDescendent(DirectUI::StrToID(TEXT("passwordpanelcontainer")))->Add(_pePwdPanel);
    if (FAILED(hr))
    {
        EndDefer(cookie);
        goto Failure;
    }

    if (_pePwdEdit)
        SetElementAccessability(_pePwdEdit, true, ROLE_SYSTEM_STATICTEXT, _pvUsername->GetString());

    _fHasPwdPanel = TRUE;

#ifdef GADGET_ENABLE_GDIPLUS
    // Ensure that the Edit control is visible
    ShowEdit();
#endif

    if (_pbPwdInfo)
    {
        // Hide hint button if no hint provided
        if (_pvHint && *(_pvHint->GetString()) != NULL)
            _pbPwdInfo->SetVisible(true);
        else
            _pbPwdInfo->SetVisible(false);
    }


    // Hide status text (do not remove or insert)
    HideStatus(0);
    HideStatus(1);

    //LayoutCheckHandler(LAYOUT_DEF_USER);
    // Push focus to edit control
    if (_pePwdEdit)
        _pePwdEdit->SetKeyFocus();

    EndDefer(cookie);

Done:

    return S_OK;

Failure:

    return hr;
}

void ShowResetWizard(HWND hw)
{

    return;
}
#define ECMAGICNUM 3212

LRESULT CALLBACK ECSubClassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uID, ULONG_PTR dwRefData)
{
    UNREFERENCED_PARAMETER(uID);
    UNREFERENCED_PARAMETER(dwRefData);
    switch (uMsg)
    {
    case WM_NOTIFY:
    {
        LPNMHDR lph;
        lph = (LPNMHDR)lParam;
        if (TTN_LINKCLICK == lph->code)
        {
            g_pErrorBalloon.HideToolTip();
            ShowResetWizard(hwnd);
            return 0;
        }
    }

    default:
        break;
    }
    return DefSubclassProc(hwnd, uMsg, wParam, lParam);
}

BOOL SubClassTheEditBox(HWND he)
{
    if (he)
    {
        //SetWindowSubclass(he, ECSubClassProc, ECMAGICNUM, NULL);
    }
    return (he != NULL);
}

void UnSubClassTheEditBox(HWND he)
{
    if (he)
    {
        //RemoveWindowSubclass(he, ECSubClassProc, ECMAGICNUM);
    }
}

HRESULT LogonAccount::RemoveCredPanel()
{
    HRESULT hr;

    if (!_fHasPwdPanel)
        goto Done;

    DWORD cookie;
    StartDefer(&cookie);

    // Clear out edit control
    if (_pePwdEdit)
    {
        _pePwdEdit->SetContentString(L"");
        UnSubClassTheEditBox(_pePwdEdit->GetHWND());     // Provide for trap of the TTN_LINKCLICK event
    }


    // Remove password panel
    hr = FindDescendent(DirectUI::StrToID(TEXT("passwordpanelcontainer")))->Remove(_pePwdPanel);
    if (FAILED(hr))
    {
        //EndDefer(cookie);
        goto Failure;
    }

    // Unhide status text
    ShowStatus(0);
    ShowStatus(1);

    _pePwdPanel = nullptr;
    _pePwdEdit = nullptr;
    _pbPwdInfo = nullptr;
    _peKbdIcon = nullptr;


    _fHasPwdPanel = FALSE;

    EndDefer(cookie);

Done:

    return S_OK;

Failure:

    return hr;
}

void LogonAccount::SetStatus(UINT nLine, LPCWSTR psz)
{
    if (psz)
    {
        _pbStatus[nLine]->SetContentString(psz);
        SetElementAccessability(_pbStatus[nLine], true, ROLE_SYSTEM_LINK, psz);
    }
}
WCHAR g_szUsername[UNLEN];

void LogonAccount::ShowPasswordIncorrectMessage()
{
    TCHAR szError[512], szTitle[128], szAccessible[640];
    BOOL fBackupAvailable = false;
    BOOL fHint = false;
    DWORD dwResult;
    g_szUsername[0] = 0;
    SubClassTheEditBox(_pePwdEdit->GetHWND());   // Provide for trap of the TTN_LINKCLICK event
    if (0 < lstrlen(_pvUsername->GetString()))
    {
        wcscpy_s(g_szUsername, _pvUsername->GetString());
        //if (0 == PRQueryStatus(NULL, _pvUsername->GetString(), &dwResult))
        //{
        //    if (0 == dwResult)
        //    {
        //        fBackupAvailable = TRUE;
        //    }
        //}
    }

    if (NULL != _pvHint && 0 < lstrlen(_pvHint->GetString()))
    {
        fHint = true;
    }

    LoadStringW(g_plf->GetHInstance(), IDS_BADPWDTITLE, szTitle, _ARRAYSIZE(szTitle));

    if (!fBackupAvailable & fHint)
        LoadStringW(g_plf->GetHInstance(), IDS_BADPWDHINT, szError, _ARRAYSIZE(szError));
    else if (fBackupAvailable & !fHint)
        LoadStringW(g_plf->GetHInstance(), IDS_BADPWDREST, szError, _ARRAYSIZE(szError));
    else if (fBackupAvailable & fHint)
        LoadStringW(g_plf->GetHInstance(), IDS_BADPWDHINTREST, szError, _ARRAYSIZE(szError));
    else
        LoadStringW(g_plf->GetHInstance(), IDS_BADPWD, szError, _ARRAYSIZE(szError));
    g_pErrorBalloon.ShowToolTip(GetModuleHandleW(NULL), _pePwdEdit->GetHWND(), szError, szTitle, TTI_ERROR, EB_WARNINGCENTERED | EB_MARKUP, 10000);

    lstrcpy(szAccessible, szTitle);
    lstrcat(szAccessible, szError);
    SetElementAccessability(_pePwdEdit, true, ROLE_SYSTEM_STATICTEXT, szAccessible);

    _pePwdEdit->RemoveLocalValue(ContentProp);
    _pePwdEdit->SetKeyFocus();
}

void LogonAccount::UpdateNotifications(BOOL fCheckEverything)
{
    HRESULT hr = E_FAIL;
    //ILogonUser* pobjUser = NULL;
    WCHAR szTemp[1024], sz[1024];

    if (_fHasPwdPanel)
        return;

    const WCHAR* pszUsername = _pvUsername->GetString();

    if (pszUsername)
    {
        //if (SUCCEEDED(hr = GetLogonUserByLogonName(pszUsername, &pobjUser)) && pobjUser)
        {
            VARIANT_BOOL vbLoggedOn;
            VARIANT varUnreadMail;
            BOOL fLoggedOn;
            int iUnreadMailCount = 0;
            DWORD dwProgramsRunning = 0;

            //if (FAILED(pobjUser->get_isLoggedOn(&vbLoggedOn)))
            {
                vbLoggedOn = VARIANT_TRUE;
            }

            fLoggedOn = (vbLoggedOn == VARIANT_TRUE);

            if (fLoggedOn)
            {
                HKEY hKey;
                //CUserProfile userProfile(pszUsername, NULL);
                //
                //if (ERROR_SUCCESS == RegOpenKeyEx(userProfile, TEXT("SessionInformation"), 0, KEY_QUERY_VALUE, &hKey))
                //{
                //    DWORD dwProgramsRunningSize = sizeof(dwProgramsRunning);
                //    RegQueryValueEx(hKey, TEXT("ProgramCount"), NULL, NULL, reinterpret_cast<LPBYTE>(&dwProgramsRunning), &dwProgramsRunningSize);
                //    RegCloseKey(hKey);
                //}
                dwProgramsRunning = 5;
            }
            SetRunningApps(dwProgramsRunning);

            if (fLoggedOn)
            {
                InsertStatus(LASS_LoggedOn);

                if (dwProgramsRunning != 0)
                {
                    LoadStringW(g_plf->GetHInstance(), (dwProgramsRunning == 1 ? IDS_RUNNINGPROGRAM : IDS_RUNNINGPROGRAMS), szTemp, ARRAYSIZE(szTemp));
                    wsprintf(sz, szTemp, dwProgramsRunning);
                    SetStatus(LASS_LoggedOn, sz);
                    ShowStatus(LASS_LoggedOn);
                }
                else
                {
                    LoadStringW(g_plf->GetHInstance(), IDS_USERLOGGEDON, szTemp, ARRAYSIZE(szTemp));
                    SetStatus(LASS_LoggedOn, szTemp);
                }
            }
            else
            {
                // if they are not logged on, clean up the logged on text and remove any padding
                RemoveStatus(LASS_LoggedOn);
            }

            if (fLoggedOn || fCheckEverything)
            {
                varUnreadMail.uintVal = 5;
                //if (FAILED(pobjUser->get_setting(L"UnreadMail", &varUnreadMail)))
                //{
                //    varUnreadMail.uintVal = 0;
                //}
                iUnreadMailCount = varUnreadMail.uintVal;

                SetUnreadMail((DWORD)iUnreadMailCount);
                if (iUnreadMailCount != 0)
                {
                    InsertStatus(LASS_Email);

                    LoadStringW(g_plf->GetHInstance(), (iUnreadMailCount == 1 ? IDS_UNREADMAIL : IDS_UNREADMAILS), szTemp, ARRAYSIZE(szTemp));
                    wsprintf(sz, szTemp, iUnreadMailCount);
                    SetStatus(LASS_Email, sz);
                    ShowStatus(LASS_Email);
                }
                else
                {
                    RemoveStatus(LASS_Email);
                }
            }

            //pobjUser->Release();
        }
    }
}

void LogonAccount::AppRunningTip()
{
    TCHAR szTitle[256], szTemp[512];

    Element* pe = FindDescendent(DirectUI::StrToID(L"username"));
    assert(pe);

    DirectUI::Value* pv;
    LPCWSTR pszDisplayName = pe->GetContentString(&pv);
    if (!pszDisplayName)
        pszDisplayName = L"";

    if (_dwRunningApps == 0)
    {
        LoadStringW(g_plf->GetHInstance(), IDS_USERISLOGGEDON, szTemp, _ARRAYSIZE(szTemp));
        wsprintf(szTitle, szTemp, pszDisplayName, _dwRunningApps);
    }
    else
    {
        LoadStringW(g_plf->GetHInstance(), (_dwRunningApps == 1 ? IDS_USERRUNNINGPROGRAM : IDS_USERRUNNINGPROGRAMS), szTemp, _ARRAYSIZE(szTemp));
        wsprintf(szTitle, szTemp, pszDisplayName, _dwRunningApps);
    }

    pv->Release();

    // get the position of the link so we can target the balloon tip correctly
    POINT pt = { 0,0 };
    CalcBalloonTargetLocation(g_plf->GetNativeHost()->GetHWND(), _pbStatus[LASS_LoggedOn], &pt);

    LoadStringW(g_plf->GetHInstance(), (_dwRunningApps > 0 ? IDS_TOOMANYPROGRAMS : IDS_TOOMANYUSERS), szTemp, _ARRAYSIZE(szTemp));
    g_pErrorBalloon.ShowToolTip(GetModuleHandleW(NULL), g_plf->GetHWND(), &pt, szTemp, szTitle, TTI_INFO, EB_WARNINGCENTERED, 10000);
}

void LogonAccount::UnreadMailTip()
{
    TCHAR szTitle[128], szMsg[1024], szTemp[512], szRes[128];
    HRESULT hr = E_FAIL;
    //ILogonUser* pobjUser = NULL;

    szMsg[0] = TEXT('\0');

    Element* pe = FindDescendent(DirectUI::StrToID(L"username"));
    assert(pe);

    DirectUI::Value* pv;
    LPCWSTR pszDisplayName = pe->GetContentString(&pv);
    if (!pszDisplayName)
        pszDisplayName = L"";

    const WCHAR* pszUsername = _pvUsername->GetString();
    DWORD dwAccountsAdded = 0;
    if (pszUsername)
    {
        //if (SUCCEEDED(hr = GetLogonUserByLogonName(pszUsername, &pobjUser)) && pobjUser)
        {
            DWORD  i, cMailAccounts;

            cMailAccounts = 5;
            for (i = 0; i < cMailAccounts; i++)
            {
                UINT cUnread = 2;
                VARIANT varAcctName = { 0 };
                varAcctName.bstrVal = (BSTR)TEXT("OI");

               // hr = pobjUser->getMailAccountInfo(i, &varAcctName, &cUnread);
                hr = S_OK;
                if (FAILED(hr))
                {
                    break;
                }

                if (varAcctName.bstrVal && cUnread > 0)
                {
                    if (dwAccountsAdded > 0)
                    {
                        lstrcat(szMsg, TEXT("\r\n"));
                    }
                    dwAccountsAdded++;
                    LoadStringW(g_plf->GetHInstance(), IDS_UNREADMAILACCOUNT, szRes, _ARRAYSIZE(szRes));
                    wsprintf(szTemp, szRes, varAcctName.bstrVal, cUnread);
                    lstrcat(szMsg, szTemp);
                }
                VariantClear(&varAcctName);
            }
            //pobjUser->Release();
        }
    }
    LoadStringW(g_plf->GetHInstance(), (_dwUnreadMail == 1 ? IDS_USERUNREADEMAIL : IDS_USERUNREADEMAILS), szTemp, _ARRAYSIZE(szTemp));
    wsprintf(szTitle, szTemp, pszDisplayName, _dwUnreadMail);
    pv->Release();

    // get the position of the link so we can target the balloon tip correctly
    POINT pt = { 0,0 };
    CalcBalloonTargetLocation(g_plf->GetNativeHost()->GetHWND(), _pbStatus[LASS_Email], &pt);

    if (szMsg[0] == 0)
    {
        LoadStringW(g_plf->GetHInstance(), IDS_UNREADMAILTEMP, szMsg, _ARRAYSIZE(szMsg));
    }
    g_pErrorBalloon.ShowToolTip(GetModuleHandleW(NULL), g_plf->GetHWND(), &pt, szMsg, szTitle, TTI_INFO, EB_WARNINGCENTERED, 10000);
}

BOOL LogonAccount::IsPasswordBlank()
{
    return FALSE;
}

