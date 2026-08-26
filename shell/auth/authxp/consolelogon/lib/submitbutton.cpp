#include "pch.h"
#include "submitbutton.h"

#include "logonaccount.h"
#include "logoninterfaces.h"

CSubmitButton::CSubmitButton() : m_owningElement(nullptr), m_fieldData(nullptr)
{
}

CSubmitButton::~CSubmitButton() = default;

DirectUI::IClassInfo* CSubmitButton::Class = nullptr;

DirectUI::IClassInfo* CSubmitButton::GetClassInfoW()
{
	return Class;
}

DirectUI::IClassInfo* CSubmitButton::GetClassInfoPtr()
{
	return Class;
}

HRESULT CSubmitButton::Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie,
                                 DirectUI::Element** ppElement)
{
	return DirectUI::CreateAndInit<CSubmitButton, int>(3, pParent, pdwDeferCookie, ppElement);
}

HRESULT CSubmitButton::Register()
{
	return DirectUI::ClassInfo<CSubmitButton, DirectUI::Button>::RegisterGlobal(HINST_THISCOMPONENT, L"SubmitButton", nullptr, 0);
}
