#pragma once
#include "pch.h"
#include "DirectUI/DirectUI.h"

class CAdvisableButton : public DirectUI::Button
	, public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
		, WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
	>
{
public:

	CAdvisableButton();
	CAdvisableButton(const CAdvisableButton& other) = delete;
	~CAdvisableButton() override;

	CAdvisableButton& operator=(const CAdvisableButton&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);
	static HRESULT Register();

	HRESULT Advise(LCPD::ICredentialField* dataSource);
	HRESULT UnAdvise();

	virtual void OnDestroy() override;
	void OnEvent(DirectUI::Event* pEvent) override;

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface
	STDMETHODIMP Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface

	int m_index;
	class CDUIUserTileElement* m_owningElement;

private:
	EventRegistrationToken m_token;
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_FieldInfo;
};
