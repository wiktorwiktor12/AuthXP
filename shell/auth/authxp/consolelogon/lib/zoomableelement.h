#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"

/*class CDUIZoomableElement : public DirectUI::Element
	, public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
		, WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
	>
{
public:

	CDUIZoomableElement();
	CDUIZoomableElement(const CDUIZoomableElement& other) = delete;
	~CDUIZoomableElement() override;

	CDUIZoomableElement& operator=(const CDUIZoomableElement&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static const DirectUI::PropertyInfo* WINAPI ElementZoomedProp();
	bool GetElementZoomed();
	HRESULT SetElementZoomed(bool v);

	HRESULT Advise(LCPD::ICredentialField* dataSource);
	HRESULT UnAdvise();

	virtual void OnDestroy() override;

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface
	STDMETHODIMP Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface

	int m_index;
	class CDUIUserTileElement* m_owningElement;

	static HRESULT Register();
private:
	EventRegistrationToken m_token;
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_FieldInfo;
};
*/
