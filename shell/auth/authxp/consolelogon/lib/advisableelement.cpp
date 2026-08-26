#include "pch.h"
#include "advisableelement.h"

#include "logonaccount.h"

CAdvisableElement::CAdvisableElement() : m_index(-1)
	, m_owningElement(nullptr)
	, m_token(0)
{
}

CAdvisableElement::~CAdvisableElement()
{
}

DirectUI::IClassInfo* CAdvisableElement::Class = nullptr;

DirectUI::IClassInfo* CAdvisableElement::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CAdvisableElement::GetClassInfoPtr()
{
	return Class;
}

HRESULT CAdvisableElement::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
                                 DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CAdvisableElement, int>(0,pParent, pdwDeferCookie, ppElement);
}

HRESULT CAdvisableElement::Register()
{
	return DirectUI::ClassInfo<CAdvisableElement, DirectUI::Element>::RegisterGlobal(HINST_THISCOMPONENT, L"AdvisableElement", nullptr, 0);
}

HRESULT CAdvisableElement::Advise(LCPD::ICredentialField* dataSource)
{
	m_FieldInfo = dataSource;

	RETURN_IF_FAILED(m_FieldInfo->add_FieldChanged(this, &m_token)); // 19
	return S_OK;
}

HRESULT CAdvisableElement::UnAdvise()
{
	if (m_FieldInfo)
	{
		RETURN_IF_FAILED(m_FieldInfo->remove_FieldChanged(m_token)); // 25

		m_FieldInfo.Reset();
	}

	return S_OK;
}

void CAdvisableElement::OnDestroy()
{
	Element::OnDestroy();
	UnAdvise();
}

HRESULT CAdvisableElement::Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args)
{
	LOG_HR_MSG(E_FAIL,"CAdvisableElement::Invoke\n");
	if (m_owningElement)
	{

		bool bShouldUpdateString = false;

		if (args == LCPD::CredentialFieldChangeKind_State)
		{
			bool bOldVisibility = GetParent()->GetVisible();
			m_owningElement->SetFieldInitialVisibility(m_FieldInfo,GetParent());
			if (bOldVisibility != GetParent()->GetVisible())
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
		//LOG_HR_MSG(E_FAIL,"CAdvisableElement::Invoke SetFieldInitialVisibility\n");
	}

	return S_OK;
}
