#include "pch.h"
#include "advisablebutton.h"

#include "usertileelement.h"

CAdvisableButton::CAdvisableButton() : m_index(-1)
	, m_owningElement(nullptr)
	, m_token(0)
{
}

CAdvisableButton::~CAdvisableButton()
{
}

DirectUI::IClassInfo* CAdvisableButton::Class = nullptr;

DirectUI::IClassInfo* CAdvisableButton::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CAdvisableButton::GetClassInfoPtr()
{
	return Class;
}

HRESULT CAdvisableButton::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
                                 DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CAdvisableButton, int>(3, pParent, pdwDeferCookie, ppElement);
}

HRESULT CAdvisableButton::Register()
{
	return DirectUI::ClassInfo<CAdvisableButton, DirectUI::Button>::RegisterGlobal(HINST_THISCOMPONENT, L"AdvisableButton", nullptr, 0);
}

HRESULT CAdvisableButton::Advise(LCPD::ICredentialField* dataSource)
{
	m_FieldInfo = dataSource;

	RETURN_IF_FAILED(m_FieldInfo->add_FieldChanged(this, &m_token)); // 19
	return S_OK;
}

HRESULT CAdvisableButton::UnAdvise()
{
	if (m_FieldInfo)
	{
		RETURN_IF_FAILED(m_FieldInfo->remove_FieldChanged(m_token)); // 25

		m_FieldInfo.Reset();
	}

	return S_OK;
}

void CAdvisableButton::OnDestroy()
{
	Button::OnDestroy();
	UnAdvise();
}

void CAdvisableButton::OnEvent(DirectUI::Event* pEvent)
{
	if (pEvent->nStage == DirectUI::GMF_DIRECT)
	{
		if (pEvent->uidType == DirectUI::Button::Click() || pEvent->uidType == DirectUI::Button::Context())
		{
			Microsoft::WRL::ComPtr<LCPD::ICommandLinkField> commandLinkField;
			if (m_FieldInfo.Get() && SUCCEEDED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&commandLinkField))))
			{
				LOG_IF_FAILED(commandLinkField->Invoke());
			}
			else
			{
				LOG_HR_MSG(E_FAIL,"m_FieldInfo->QueryInterface failed");
			}
		}
	}

	Button::OnEvent(pEvent);
}

HRESULT CAdvisableButton::Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args)
{
	LOG_HR_MSG(E_FAIL,"CAdvisableButton::Invoke\n");
	if (m_owningElement && m_owningElement->m_containersArray[m_index])
	{
		CFieldWrapper* fieldData;
		m_owningElement->fieldsArray.GetAt(m_index,fieldData);

		bool bShouldUpdateString = false;

		if (args == LCPD::CredentialFieldChangeKind_State)
		{
			bool bOldVisibility = GetVisible();
			m_owningElement->SetFieldVisibility(m_index,m_FieldInfo);
			if (bOldVisibility != GetVisible())
				bShouldUpdateString = true;
		}
		else if (args == LCPD::CredentialFieldChangeKind_SetString)
		{
			//m_owningElement->SetFieldVisibility(m_owningElement->m_containersArray[m_index],fieldData);
			bShouldUpdateString = true;
		}
		if (bShouldUpdateString)
		{
			Microsoft::WRL::Wrappers::HString label;
			Microsoft::WRL::ComPtr<LCPD::ICommandLinkField> commandLinkField;
			if (SUCCEEDED(m_FieldInfo->QueryInterface(IID_PPV_ARGS(&commandLinkField))))
			{
				RETURN_IF_FAILED(commandLinkField->get_Content(label.ReleaseAndGetAddressOf()));
				BOOLEAN bStyledAsButton = FALSE;
#if CONSOLELOGON_FOR >= CONSOLELOGON_FOR_19h1
				RETURN_IF_FAILED(commandLinkField->get_IsStyledAsButton(&bStyledAsButton));
#endif

				LOG_HR_MSG(E_FAIL,"bStyledAsButton %i", bStyledAsButton ? 1 : 0);
			}
			else
				RETURN_IF_FAILED(m_FieldInfo->get_Label(label.ReleaseAndGetAddressOf()));

			if (label.Length() > 0)
			{
				RETURN_IF_FAILED(SetContentString(label.GetRawBuffer(nullptr)));
				RETURN_IF_FAILED(SetAccName(label.GetRawBuffer(nullptr)));
			}
		}
		//m_owningElement->SetFieldInitialVisibility(m_owningElement->m_containersArray[m_index],fieldData);
		//LOG_HR_MSG(E_FAIL,"CAdvisableButton::Invoke SetFieldInitialVisibility\n");
	}

	return S_OK;
}
