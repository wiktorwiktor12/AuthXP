#pragma once
#include "pch.h"
#include "DirectUI/DirectUI.h"

class CSubmitButton : public DirectUI::Button
{
public:

	CSubmitButton();
	CSubmitButton(const CSubmitButton& other) = delete;
	~CSubmitButton() override;

	CSubmitButton& operator=(const CSubmitButton&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);
	static HRESULT Register();

	class LogonAccount* m_owningElement;
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_fieldData;
};
