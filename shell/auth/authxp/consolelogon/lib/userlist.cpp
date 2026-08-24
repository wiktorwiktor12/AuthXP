#include "pch.h"
#include "userlist.h"

#include "duiutil.h"
#include "logonframe.h"
#include "restrictededit.h"
#include "zoomableelement.h"

/*DirectUI::IClassInfo* UserList::Class = nullptr;

DirectUI::IClassInfo* UserList::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* UserList::GetClassInfoPtr()
{
	return Class;
}

HRESULT UserList::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<UserList, int>(0, pParent, pdwDeferCookie, ppElement);
}

HRESULT UserList::Register()
{
	return DirectUI::ClassInfo<UserList, DirectUI::Element>::RegisterGlobal(HINST_THISCOMPONENT, L"UserList", nullptr, 0);
}

HRESULT UserList::Configure(DirectUI::DUIXmlParser* parser)
{
	m_xmlParser = parser;

	RETURN_IF_FAILED(m_xmlParser->CreateElement(L"UserListTemplate",nullptr,this,nullptr,&m_UserListContainer));

	RETURN_IF_FAILED(Add(m_UserListContainer));

	m_UserListSelector = (DirectUI::Selector*)m_UserListContainer->FindDescendent(DirectUI::StrToID(L"UserList"));

	return S_OK;
}

HRESULT UserList::AddTileFromData(const Microsoft::WRL::ComPtr<LCPD::ICredential>& credential, const Microsoft::WRL::ComPtr<LCPD::IUser> user, HSTRING userName)
{
	CDUIUserTileElement* userTile;
	RETURN_IF_FAILED(m_xmlParser->CreateElement(L"UserTileTemplate", nullptr, this, nullptr, (DirectUI::Element**)&userTile));

	auto scopeExit = wil::scope_exit([&]() -> void {userTile->Destroy(true);});

	userTile->m_scenario = LCPD::CredProvScenario_Logon;
	userTile->m_owningUserList = this;
	userTile->m_dataSourceCredential = credential;
	userTile->m_dataSourceUser = user;

	//userTile->SetActive(7);
	//userTile->SetActive(bIsActive ? 3 : 0);

	Microsoft::WRL::ComPtr<WFC::IVectorView<LCPD::ICredentialField*>> fields;
	RETURN_IF_FAILED(credential->get_Fields(&fields));

	UINT numFields;
	RETURN_IF_FAILED(fields->get_Size(&numFields));

	for (int i = numFields - 1; i >= 0; --i)
	{
		Microsoft::WRL::ComPtr<LCPD::ICredentialField> dataSource;
		RETURN_IF_FAILED(fields->GetAt(i, &dataSource));

		CFieldWrapper* fieldWrapper = DirectUI::HNewAndZero<CFieldWrapper>();
		fieldWrapper->m_dataSourceCredentialField = dataSource;

		BOOLEAN isVisibleInSelectedTile = FALSE;
		RETURN_IF_FAILED(dataSource->get_IsVisibleInSelectedTile(&isVisibleInSelectedTile));

		fieldWrapper->m_isSelectorField = false;

		RETURN_IF_FAILED(userTile->fieldsArray.Add(fieldWrapper));
	}

	if (CLogonFrame7::GetSingleton()->m_currentReason != LC::LogonUIRequestReason_LogonUIChange)
	{
		Microsoft::WRL::Wrappers::HString status;
		if (user.Get())
			RETURN_IF_FAILED(user->get_LogonStatus(status.ReleaseAndGetAddressOf()));

		if (user.Get())
		{
			CFieldWrapper* statusField = DirectUI::HNewAndZero<CFieldWrapper>();
			statusField->m_isSelectorField = true;
			statusField->m_label.Initialize(status.GetRawBuffer(nullptr));
			statusField->m_size = LCPD::CredentialTextSize_Small;

			RETURN_IF_FAILED(userTile->fieldsArray.Add(statusField));
		}

		CFieldWrapper* usernameField = DirectUI::HNewAndZero<CFieldWrapper>();
		usernameField->m_isSelectorField = true;
		usernameField->m_label.Initialize(WindowsGetStringRawBuffer(userName, nullptr));
		usernameField->m_size = LCPD::CredentialTextSize_Small;

		RETURN_IF_FAILED(userTile->fieldsArray.Add(usernameField));

		if (user.Get())
		{
			CFieldWrapper* zoomedStatusField = DirectUI::HNewAndZero<CFieldWrapper>();
			zoomedStatusField->m_isSelectorField = false;
			zoomedStatusField->m_label.Initialize(status.GetRawBuffer(nullptr));
			zoomedStatusField->m_size = LCPD::CredentialTextSize_Small;

			RETURN_IF_FAILED(userTile->fieldsArray.Add(zoomedStatusField));
		}

		CFieldWrapper* zoomedUsernameField = DirectUI::HNewAndZero<CFieldWrapper>();
		zoomedUsernameField->m_isSelectorField = false;
		zoomedUsernameField->m_label.Initialize(WindowsGetStringRawBuffer(userName, nullptr));
		zoomedUsernameField->m_size = LCPD::CredentialTextSize_Large;

		RETURN_IF_FAILED(userTile->fieldsArray.Add(zoomedUsernameField));
	}

	int width = 180;
	userTile->SetWidth(MulDiv(width, GetScreenDPI(), 96));
	userTile->m_xmlParser = m_xmlParser;
	userTile->m_capsLockWarning = userTile->FindDescendent(DirectUI::StrToID(L"CapsLockWarning"));

	RETURN_IF_FAILED(userTile->_CreateFieldsForDeselected());
	RETURN_IF_FAILED(userTile->SetVisible(true));
	RETURN_IF_FAILED(m_UserListSelector->Add(userTile));
	//RETURN_IF_FAILED(UserListSelector->Insert(userTile,0));

	_SetUnzoomedWidth();

	scopeExit.release();

	return S_OK;
}

void UserList::DeselectAllTiles()
{
	m_UserListSelector->RemoveLocalValue(DirectUI::Selector::SelectionProp);
}

void UserList::HideAllTiles()
{
	DWORD cookie;
	StartDefer(&cookie);

	//todo: handle that active tile thing

	DeselectAllTiles();

	DirectUI::Value* childval;
	auto Children = m_UserListSelector->GetChildren(&childval);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			auto Child = Children->GetItem(i);
			if (Child)
				Child->SetVisible(false);
		}
	}
	childval->Release();

	if (cookie)
		EndDefer(cookie);
}

void UserList::FindAndSetKeyFocus()
{
	DirectUI::Value* childval;
	auto Children = m_UserListSelector->GetChildren(&childval);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			//bool SetVisible = visibleIndex == -1 || visibleIndex == i;

			auto Child = Children->GetItem(i);
			if (Child)
			{
				m_UserListSelector->SetSelection(Child);
				Child->SetKeyFocus();
				break;
			}
		}
	}

	childval->Release();
}

void UserList::EnableList()
{
	SetEnabled(true);
	m_UserListSelector->SetEnabled(true);
	m_UserListContainer->SetEnabled(true);
}

void UserList::DestroyAllTiles()
{
	DWORD cookie;
	StartDefer(&cookie);

	m_UserListSelector->DestroyAll(true);

	EndDefer(cookie);
}

CDUIUserTileElement* UserList::FindTileByCredential(const Microsoft::WRL::ComPtr<LCPD::ICredential>& credential)
{
	CDUIUserTileElement* ret = nullptr;

	DirectUI::Value* childVal;
	auto Children = m_UserListSelector->GetChildren(&childVal);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			auto Child = (CDUIUserTileElement*)Children->GetItem(i);
			if (Child->m_dataSourceCredential == credential)
			{
				ret = Child;
				break;
			}
		}
	}
	childVal->Release();

	return ret;
}

HRESULT STDMETHODCALLTYPE SetOneElementZoomed(DirectUI::Element* a1, LPVOID a2)
{
	if (IsElementOfClass(a1, L"ZoomableElement"))
	{
		DirectUI::Value* Bool = DirectUI::Value::CreateBool((unsigned long long)a2 == 1);
		if (Bool)
		{
			a1->SetValue(CDUIZoomableElement::ElementZoomedProp, 1, Bool);
			Bool->Release();
		}
	}
	return S_OK;
}


HRESULT UserList::ZoomTile(CDUIUserTileElement* userTile)
{
	if (userTile->m_bTileZoomed)
		return S_OK;

    DWORD cookie;
    StartDefer(&cookie);

    int index = -2;

	DirectUI::Value* childVal;
	auto Children = m_UserListSelector->GetChildren(&childVal);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			auto Child = (CDUIUserTileElement*)Children->GetItem(i);
            if (Child == userTile)
            {
                index = i;
                break;
            }
		}
	}
    childVal->Release();

    _ShowEnumeratedTilesWorker(index);

    RETURN_IF_FAILED(userTile->_CreateFieldsForSelected());

	CDUIRestrictedEdit* EditFieldToFocus = nullptr;

    for (int i = 0; i < userTile->fieldsArray.GetSize(); ++i)
    {
    	CFieldWrapper* fieldData;
    	RETURN_IF_FAILED(userTile->fieldsArray.GetAt(i,fieldData));

    	LCPD::CredentialFieldKind kind = LCPD::CredentialFieldKind_StaticText;
    	BOOLEAN bHidden = FALSE;
    	BOOLEAN isVisibleInSelectedTile = TRUE;
    	if (fieldData->m_dataSourceCredentialField)
    	{
    		RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_Kind(&kind));
    		RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_IsHidden(&bHidden));
    		RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_IsVisibleInSelectedTile(&isVisibleInSelectedTile));
    	}

        //bool bShouldHide = fieldData->m_isSelectorField && kind != LCPD::CredentialFieldKind_TileImage || (!(!bHidden && isVisibleInSelectedTile != 0) && kind != LCPD::CredentialFieldKind_TileImage);
        bool bShouldHide = kind != LCPD::CredentialFieldKind_TileImage && (fieldData->m_isSelectorField || (!(!bHidden && isVisibleInSelectedTile != 0)));

        DirectUI::Element* element = userTile->m_elementsArray[i];
        if (userTile->m_containersArray[i])
        {
			SetLayoutPosDownTree(!bShouldHide ? -1 : -3, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
			SetVisibleDownTree(!bShouldHide, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
			SetEnabledDownTree(!bShouldHide, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
        }
        else if (element)
        {
            element->SetLayoutPos(!bShouldHide ? -1 : -3);
            element->SetVisible(!bShouldHide);
            element->SetEnabled(!bShouldHide);
            //todo: high contrast stuff
        }

    	if (element && !bHidden && kind == LCPD::CredentialFieldKind_EditText)
    	{
    		EditFieldToFocus = (CDUIRestrictedEdit*)element;
    	}
    }

    userTile->SetTileZoomed(true);
    userTile->m_bTileZoomed = true;

    userTile->SetWidth(GetSystemMetrics(SM_CXSCREEN));
	userTile->m_capsLockWarning->SetLayoutPos(-3);
	m_UserListSelector->SetWidth(GetSystemMetrics(SM_CXSCREEN));

    DWORD cookie2;
    userTile->StartDefer(&cookie2);

    unsigned long long val = 1;

    TraverseTree(userTile, DirectUI::Element::GetClassInfoPtr(), SetOneElementZoomed, (LPVOID)val);

    if (cookie2)
        userTile->EndDefer(cookie2);

    SetVisibleDownTree(true,userTile,userTile->FindDescendent(DirectUI::StrToID(L"NonSelectorFieldsFrame")));
    SetEnabledDownTree(true,userTile,userTile->FindDescendent(DirectUI::StrToID(L"NonSelectorFieldsFrame")));

    //todo: password field text setkeyfocus

	if (EditFieldToFocus)
	{
		//userTile->SetKeyFocus();
		EditFieldToFocus->SetKeyFocus();
		//EditFieldToFocus->SetActive(7);
		SendMessageW(EditFieldToFocus->GetHWND(), 0xB1u, 0, -1);
	}

    //CLogonFrame7::GetSingleton()->SetOptions(257 & ~0x20u);
	if (CLogonFrame7::GetSingleton()->m_currentReason == LC::LogonUIRequestReason_LogonUIChange)
		CLogonFrame7::GetSingleton()->SetOptions(MessageOptionFlag::Cancel | MessageOptionFlag::Accessibility | MessageOptionFlag::ShutDownFrame);
	else
		CLogonFrame7::GetSingleton()->SetOptions(MessageOptionFlag::SwitchUser | MessageOptionFlag::Accessibility | MessageOptionFlag::ShutDownFrame);

	//CLogonFrame7::GetSingleton()->m_nativeHost->Host(m_UserListSelector);
	//CLogonFrame7::GetSingleton()->m_nativeHost->Host(CLogonFrame7::GetSingleton());

    if (cookie)
        EndDefer(cookie);

	return S_OK;
}

HRESULT UserList::UnzoomList(CDUIUserTileElement* userTile)
{
	if (!userTile)
		return E_INVALIDARG;

	_ShowEnumeratedTilesWorker(-1);

	m_UserListSelector->SetSelection(userTile);
	userTile->SetKeyFocus();

	userTile->SetTileZoomed(false);
	userTile->m_bTileZoomed = false;

	userTile->SetWidth(MulDiv(180, GetScreenDPI(), 96));

	for (int i = 0; i < userTile->fieldsArray.GetSize(); ++i)
	{
		CFieldWrapper* fieldData;
		RETURN_IF_FAILED(userTile->fieldsArray.GetAt(i,fieldData));

		LCPD::CredentialFieldKind kind = LCPD::CredentialFieldKind_StaticText;
		BOOLEAN bHidden = FALSE;
		BOOLEAN isVisibleInDeselectedTile = TRUE;
		if (fieldData->m_dataSourceCredentialField)
		{
			RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_Kind(&kind));
			RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_IsHidden(&bHidden));
			RETURN_IF_FAILED(fieldData->m_dataSourceCredentialField->get_IsVisibleInDeselectedTile(&isVisibleInDeselectedTile));
		}

		//bool bShouldHide = !fieldData->m_isSelectorField && kind != LCPD::CredentialFieldKind_TileImage || (!(!bHidden && isVisibleInDeselectedTile != 0) && kind != LCPD::CredentialFieldKind_TileImage);
		bool bShouldHide = kind != LCPD::CredentialFieldKind_TileImage && (!fieldData->m_isSelectorField || (!(!bHidden && isVisibleInDeselectedTile != 0)));

		DirectUI::Element* element = userTile->m_elementsArray[i];
		if (userTile->m_containersArray[i])
		{
			SetLayoutPosDownTree(!bShouldHide ? -1 : -3, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
			SetVisibleDownTree(!bShouldHide, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
			SetEnabledDownTree(!bShouldHide, userTile->m_containersArray[i], userTile->m_elementsArray[i]);
		}
		else if (element)
		{
			element->SetLayoutPos(!bShouldHide ? -1 : -3);
			element->SetVisible(!bShouldHide ? -1 : -3);
			element->SetEnabled(!bShouldHide ? -1 : -3);
			//todo: high contrast stuff
		}
	}

	unsigned long long val = 0;

	TraverseTree(userTile, DirectUI::Element::GetClassInfoPtr(), SetOneElementZoomed, (LPVOID)val);

	SetVisibleDownTree(false, userTile, userTile->FindDescendent(DirectUI::StrToID(L"NonSelectorFieldsFrame")));
	SetEnabledDownTree(false, userTile, userTile->FindDescendent(DirectUI::StrToID(L"NonSelectorFieldsFrame")));

	SetVisibleDownTree(true, userTile, userTile->FindDescendent(DirectUI::StrToID(L"SelectorFieldFrame")));
	SetEnabledDownTree(true, userTile, userTile->FindDescendent(DirectUI::StrToID(L"SelectorFieldFrame")));

	//todo: low res mode

	_SetUnzoomedWidth();

	CLogonFrame7::GetSingleton()->SetOptions(MessageOptionFlag::Accessibility | MessageOptionFlag::ShutDownFrame);

	return S_OK;
}

CDUIUserTileElement* UserList::GetZoomedTile()
{
	CDUIUserTileElement* ret = nullptr;

	DirectUI::Value* childVal;
	auto Children = m_UserListSelector->GetChildren(&childVal);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			auto Child = (CDUIUserTileElement*)Children->GetItem(i);
			if (Child->m_bTileZoomed == true)
			{
				ret = Child;
				break;
			}
		}
	}
	childVal->Release();

	return ret;
}
UINT sizeArray[] = { 1,1,2,3,4,5,3,4,4,5,5,4,4,5,5 };
void UserList::_SetUnzoomedWidth()
{
	DirectUI::Value* childVal;
	auto children = m_UserListSelector->GetChildren(&childVal);

	UINT numTiles = 0;
	if (children)
		numTiles = children->GetSize() & 0xFFFFFFF;

	childVal->Release();

	UINT size = 0;
	if (numTiles >= 16)
		size = 5;
	else
		size = sizeArray[numTiles];

	DirectUI::Element* InsideFrame = GetRoot()->FindDescendent(DirectUI::StrToID(L"InsideFrame"));

	DirectUI::Value* extent;
	LONG xExtent = InsideFrame->GetExtent(&extent)->cx;
	extent->Release();

	int dpi = GetScreenDPI();
	int nNumber = 180 * size + 12;
	while (xExtent < MulDiv(nNumber, dpi, 96))
	{
		--size;
		nNumber -= 180;
		xExtent = InsideFrame->GetExtent(&extent)->cx;
		extent->Release();
		dpi = GetScreenDPI(); // no clue why they do this
	}
	int width = MulDiv(180 * size + 12, GetScreenDPI(), 96);
	m_UserListSelector->SetWidth(width);
}

void UserList::_ShowEnumeratedTilesWorker(int visibleIndex)
{
	DWORD cookie;
	StartDefer(&cookie);

	DirectUI::Value* childval;
	auto Children = m_UserListSelector->GetChildren(&childval);
	if (Children)
	{
		for (UINT i = 0; i < Children->GetSize(); ++i)
		{
			bool SetVisible = visibleIndex == -1 || visibleIndex == i;

			auto Child = Children->GetItem(i);
			if (Child)
				Child->SetVisible(SetVisible);
		}
	}

	childval->Release();

	if (cookie)
		EndDefer(cookie);
}
*/
