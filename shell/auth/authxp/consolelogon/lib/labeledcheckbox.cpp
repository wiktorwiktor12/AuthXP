#include "pch.h"
#include "labeledcheckbox.h"

#include "duiutil.h"

static BYTE _uidCDUILabeledCheckboxToggled = 0;
UID CDUILabeledCheckbox::s_Toggled = UID(&_uidCDUILabeledCheckboxToggled);

DirectUI::IClassInfo* CDUILabeledCheckbox::Class = nullptr;

CDUILabeledCheckbox::CDUILabeledCheckbox() : m_checkbox(nullptr)
	, m_label(nullptr)
	, m_checkedBackgroundFill(nullptr)
	, m_uncheckedBackgroundFill(nullptr)
	, m_isChecked(FALSE)
	//, m_index(-1)
	//, m_owningElement(nullptr)
	, m_token(0)
{
}

CDUILabeledCheckbox::~CDUILabeledCheckbox()
{
}

DirectUI::IClassInfo* CDUILabeledCheckbox::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CDUILabeledCheckbox::GetClassInfoPtr()
{
	return Class;
}

HRESULT CDUILabeledCheckbox::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
	DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CDUILabeledCheckbox, int>(1, pParent, pdwDeferCookie, ppElement);
}

HRESULT CDUILabeledCheckbox::Register()
{
	return DirectUI::ClassInfo<CDUILabeledCheckbox, DirectUI::Button>::RegisterGlobal(HINST_THISCOMPONENT, L"DUILabeledCheckbox", nullptr, 0);
}

HRESULT CDUILabeledCheckbox::Configure(bool isChecked, const wchar_t* labelText)
{
	m_checkbox = FindDescendent(DirectUI::StrToID(L"Checkbox"));
	m_label = FindDescendent(DirectUI::StrToID(L"Label"));

	DWORD cookie;
	StartDefer(&cookie);

	auto scopeExit = wil::scope_exit([&]() -> void {if (cookie) { EndDefer(cookie); }});

	RETURN_IF_FAILED(SetContentAndAcc(m_label,labelText));

	RETURN_IF_FAILED(m_label->SetClass(L"SmallText"));

	m_checkedBackgroundFill = DirectUI::Value::CreateDFCFill(4u, 0x400u);
	if (m_checkedBackgroundFill)
	{
		m_uncheckedBackgroundFill = DirectUI::Value::CreateDFCFill(4u, 0);

		if (m_uncheckedBackgroundFill)
		{
			SetChecked(isChecked);
		}
		else
		{
			m_checkedBackgroundFill->Release();
			m_checkedBackgroundFill = nullptr;
		}
	}

	if (m_checkedBackgroundFill && m_uncheckedBackgroundFill)
	{
		RETURN_IF_FAILED(m_checkbox->SetAccessible(true));

		RETURN_IF_FAILED(m_checkbox->SetAccRole(44));

		RETURN_IF_FAILED(DirectUI::SetDefAction(m_checkbox,(unsigned int)isChecked - 791));

		RETURN_IF_FAILED(m_checkbox->SetAccName(labelText));
	}

	return S_OK;
}

void CDUILabeledCheckbox::SetChecked(bool newIsChecked, bool bInformCred)
{
	m_isChecked = newIsChecked;

	DWORD cookie;
	StartDefer(&cookie);

	DirectUI::Value* fillToUse;
	if (m_isChecked)
		fillToUse = m_checkedBackgroundFill;
	else
		fillToUse = m_uncheckedBackgroundFill;

	m_checkbox->SetValue(BackgroundProp,1,fillToUse);

	if (cookie)
		EndDefer(cookie);

	int AccState = m_checkbox->GetAccState();

	int unCheckedAccstate = AccState & 0xFFFFFFEF;
	int checkedAccstate = AccState | 0x10;

	if (m_isChecked)
		m_checkbox->SetAccState(checkedAccstate);
	else
		m_checkbox->SetAccState(unCheckedAccstate);

	DirectUI::SetDefAction(m_checkbox, (unsigned int)(LOBYTE(m_isChecked) != 0) - 791);

	if (m_FieldInfo.Get() && bInformCred)
	{
		Microsoft::WRL::ComPtr<LCPD::ICheckBoxField> checkboxField;
		if (SUCCEEDED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&checkboxField))))
		{
			LOG_IF_FAILED(checkboxField->put_Checked(m_isChecked));
		}
		else
		{
			LOG_HR_MSG(E_FAIL,"Failed to get checkboxfield");
		}
	}
}

void CDUILabeledCheckbox::OnInput(DirectUI::InputEvent* inputEvent)
{
	if (inputEvent->nStage > DirectUI::GMF_ROUTED)
		return DirectUI::Button::OnInput(inputEvent);

	//bool bShouldUpdateCredField = false;

	if (inputEvent->nDevice == DirectUI::GINPUT_KEYBOARD)
	{
		auto keyboardEvent = reinterpret_cast<DirectUI::KeyboardEvent*>(inputEvent);
		if ((keyboardEvent->ch == 13 || keyboardEvent->ch == 32) && keyboardEvent->nCode)
		{
			if (inputEvent->nCode == DirectUI::GMOUSE_DOWN)
			{
				SetChecked(m_isChecked == 0, true);

				DirectUI::Event event;
				event.fHandled = m_isChecked;
				event.fUIAHandled = m_isChecked;
				event.peTarget = this;
				event.uidType = CDUILabeledCheckbox::s_Toggled;
				DirectUI::Element::FireEvent(&event, true, false);
			}
		}
	}
	else if (inputEvent->nDevice == DirectUI::GINPUT_MOUSE)
	{
		auto mouseEvent = reinterpret_cast<DirectUI::MouseEvent*>(inputEvent);
		if ((mouseEvent->bButton == 1 || mouseEvent->bButton == 2) && inputEvent->nCode != DirectUI::GMOUSE_DOWN)
		{
			if (inputEvent->nCode == DirectUI::GMOUSE_UP)
			{
				SetChecked(m_isChecked == 0, true);

				DirectUI::Event event;
				event.fHandled = m_isChecked;
				event.fUIAHandled = m_isChecked;
				event.peTarget = this;
				event.uidType = CDUILabeledCheckbox::s_Toggled;
				DirectUI::Element::FireEvent(&event, true, false);
			}
		}
	}

	return DirectUI::Button::OnInput(inputEvent);
}

void CDUILabeledCheckbox::SetKeyFocus()
{
	if (GetVisible() && GetEnabled()) // guess
		m_checkbox->SetKeyFocus();
}

HRESULT CDUILabeledCheckbox::Advise(LCPD::ICredentialField* dataSource)
{
	m_FieldInfo = dataSource;

	RETURN_IF_FAILED(m_FieldInfo->add_FieldChanged(this, &m_token)); // 19
	return S_OK;
}

HRESULT CDUILabeledCheckbox::UnAdvise()
{
	if (m_FieldInfo)
	{
		RETURN_IF_FAILED(m_FieldInfo->remove_FieldChanged(m_token)); // 25

		m_FieldInfo.Reset();
	}

	return S_OK;
}

void CDUILabeledCheckbox::OnDestroy()
{
	Button::OnDestroy();
	UnAdvise();
}

HRESULT CDUILabeledCheckbox::Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args)
{
	LOG_HR_MSG(E_FAIL,"CDUILabeledCheckbox::Invoke\n");
	//if (m_owningElement && m_owningElement->m_containersArray[m_index])
	{
		//CFieldWrapper* fieldData;
		//m_owningElement->fieldsArray.GetAt(m_index,fieldData);

		bool bShouldUpdateString = false;

		if (args == LCPD::CredentialFieldChangeKind_SetString || args == LCPD::CredentialFieldChangeKind_SetCheckbox || args == LCPD::CredentialFieldChangeKind_State)
		{
			bool bOldVisibility = GetVisible();

			LCPD::CredentialFieldKind kind = LCPD::CredentialFieldKind_StaticText;
			if (m_FieldInfo.Get() != nullptr)
				RETURN_IF_FAILED(m_FieldInfo->get_Kind(&kind));

			BOOLEAN bIsVisibleInDeselectedTile = TRUE;
			BOOLEAN bIsVisibleInSelectedTile = TRUE;
			BOOLEAN isHidden = FALSE;

			if (m_FieldInfo.Get() != nullptr)
			{
				RETURN_IF_FAILED(m_FieldInfo->get_IsVisibleInDeselectedTile(&bIsVisibleInDeselectedTile));
				RETURN_IF_FAILED(m_FieldInfo->get_IsVisibleInSelectedTile(&bIsVisibleInSelectedTile));
				RETURN_IF_FAILED(m_FieldInfo->get_IsHidden(&isHidden));
			}

			if (m_FieldInfo.Get() != nullptr)
			{
				LOG_HR_MSG(E_FAIL, "CDUILabeledCheckbox::SetFieldVisibility isHidden %i",isHidden ? 1 : 0);
				LOG_HR_MSG(E_FAIL, "CDUILabeledCheckbox::SetFieldVisibility bIsVisibleInSelectedTile %i",bIsVisibleInSelectedTile ? 1 : 0);
			}

			//isVisible = true;
			bool isVisible = !isHidden && bIsVisibleInSelectedTile != 0;

			if (isVisible != bOldVisibility)
			{
				SetLayoutPos(isVisible ? DirectUI::LP_Auto : DirectUI::LP_None);
				SetVisible(isVisible);
			}

			if (isVisible != bOldVisibility || args != LCPD::CredentialFieldChangeKind_State)
				bShouldUpdateString = true;
		}
		if (bShouldUpdateString)
		{
			Microsoft::WRL::Wrappers::HString label;
			Microsoft::WRL::ComPtr<LCPD::ICheckBoxField> checkBoxField;
			if (SUCCEEDED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&checkBoxField))))
			{
				BOOLEAN bIsChecked = FALSE;
				RETURN_IF_FAILED(checkBoxField->get_Checked(&bIsChecked));
				SetChecked(bIsChecked);
			}

			RETURN_IF_FAILED(m_FieldInfo->get_Label(label.ReleaseAndGetAddressOf()));

			if (label.Length() > 0)
			{
				RETURN_IF_FAILED(SetContentAndAcc(m_label,label.GetRawBuffer(nullptr)));
			}
		}
		//m_owningElement->SetFieldInitialVisibility(m_owningElement->m_containersArray[m_index],fieldData);
		//LOG_HR_MSG(E_FAIL,"CDUILabeledCheckbox::Invoke SetFieldInitialVisibility\n");
	}

	return S_OK;
}
