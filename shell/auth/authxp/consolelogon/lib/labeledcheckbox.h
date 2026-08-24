#pragma once

#include "pch.h"
#include "DirectUI/DirectUI.h"

class CDUILabeledCheckbox : public DirectUI::Button
	, public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>
		, WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind>
	>
{
public:

	CDUILabeledCheckbox();
	CDUILabeledCheckbox(const CDUILabeledCheckbox& other) = delete;
	~CDUILabeledCheckbox() override;

	CDUILabeledCheckbox& operator=(const CDUILabeledCheckbox&) = delete;

	static DirectUI::IClassInfo* Class;
	DirectUI::IClassInfo* GetClassInfoW() override;
	static DirectUI::IClassInfo* GetClassInfoPtr();

	static HRESULT Create(DirectUI::Element* pParent, unsigned long* pdwDeferCookie, DirectUI::Element** ppElement);

	static HRESULT Register();

	HRESULT Configure(bool isChecked, const wchar_t* labelText);
	void SetChecked(bool isChecked, bool bInformCred = false);
	void OnInput(DirectUI::InputEvent* inputEvent) override;
	void SetKeyFocus() override;

	HRESULT Advise(LCPD::ICredentialField* dataSource);
	HRESULT UnAdvise();
	virtual void OnDestroy() override;

	//~ Begin WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface
	STDMETHODIMP Invoke(LCPD::ICredentialField* sender, LCPD::CredentialFieldChangeKind args) override;
	//~ End WF::ITypedEventHandler<LCPD::ICredentialField*, LCPD::CredentialFieldChangeKind> Interface

	static UID s_Toggled;

	DirectUI::Element* m_checkbox;
	DirectUI::Element* m_label;
	DirectUI::Value* m_checkedBackgroundFill;
	DirectUI::Value* m_uncheckedBackgroundFill;
	BOOL m_isChecked;

	//int m_index;
	//class CDUIUserTileElement* m_owningElement;

private:
	EventRegistrationToken m_token;
	Microsoft::WRL::ComPtr<LCPD::ICredentialField> m_FieldInfo;
};
